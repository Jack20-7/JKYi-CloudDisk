#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include<time.h>

#include "make_log.h"
#include "util_cgi.h"
#include "deal_mysql.h"
#include "redis_op.h"
#include "cfg.h"
#include "des.h"
#include "cJSON.h"
#include "md5.h"
#include "base64.h"

#include "fcgi_config.h"
#include "fcgi_stdio.h"

#define LOGIN_LOG_MODULE "cgi"
#define LOGIN_LOG_PROC   "login"


int get_login_info(char* login_buf,char* username,char* pwd){
    int ret = 0;
    cJSON* root = cJSON_Parse(login_buf);
    
    if(NULL == root){
        LOG(LOGIN_LOG_MODULE,LOGIN_LOG_PROC,"cJSON_Parse err\n");
        ret = -1;
        goto END;
    }

    cJSON* child1 = cJSON_GetObjectItem(root,"user");
    if(NULL == child1){
        LOG(LOGIN_LOG_MODULE,LOGIN_LOG_PROC,"get user err\n");
        ret = -1;
        goto END;
    }
    strcpy(username,child1->valuestring);

    cJSON* child2 = cJSON_GetObjectItem(root,"pwd");
    if(NULL == child2){
        LOG(LOGIN_LOG_MODULE,LOGIN_LOG_PROC,"get pwd err\n");
        ret = -1;
        goto END;
    }
    strcpy(pwd,child2->valuestring);

    LOG(LOGIN_LOG_MODULE,LOGIN_LOG_PROC,"user = %s,pwd = %s\n",username,pwd);

END:
   if(root != NULL){
    cJSON_Delete(root);
    root = NULL;
   }

   return ret;
}

int check_user_pwd(char* username,char*pwd){
    char sql_cmd[SQL_MAX_LEN] = {0};
    int ret = 0;

    char mysql_user[256] = {0};
    char mysql_pwd[256] = {0};
    char mysql_db[256] = {0};
    if(get_cfg_value(CFG_PATH,"mysql","user",mysql_user) != 0){
        LOG(LOGIN_LOG_MODULE,LOGIN_LOG_PROC,"get mysql user");
        return -1;
    }
    if(get_cfg_value(CFG_PATH,"mysql","password",mysql_pwd) != 0){
        LOG(LOGIN_LOG_MODULE,LOGIN_LOG_PROC,"get mysql password");
        return -1;
    }
    if(get_cfg_value(CFG_PATH,"mysql","database",mysql_db) != 0){
        LOG(LOGIN_LOG_MODULE,LOGIN_LOG_PROC,"get mysql database");
        return -1;
    }
    LOG(LOGIN_LOG_MODULE,LOGIN_LOG_PROC,"mysql_user = %s,mysql_pwd = %s,mysql_db = %s",mysql_user,mysql_pwd,mysql_db);

    MYSQL* conn = mysql_conn(mysql_user,mysql_pwd,mysql_db);
    if(NULL == conn){
        LOG(LOGIN_LOG_MODULE,LOGIN_LOG_PROC,"mysql_conn err\n");
        return -1;
    }

   mysql_query(conn,"set names utf8");

   sprintf(sql_cmd,"select password from user_info where user_name = '%s'",username);
   char tmp[PWD_LEN] = {0};

   process_result_one(conn,sql_cmd,tmp);
   if(strcmp(tmp,pwd) == 0){
    ret = 0;
   }else{
    ret = -1;
   }
   mysql_close(conn);

  return ret;
}

//生成token，保存在redis中.并且还会设置过期时间.方便免密登录
int set_token(char* username,char* token){
    int ret = 0;
    redisContext* redis_conn = NULL;


    char redis_ip[30] = {0};
    char redis_port[10] = {0};

    get_cfg_value(CFG_PATH,"redis","ip",redis_ip);
    get_cfg_value(CFG_PATH,"redis","port",redis_port);
    LOG(LOGIN_LOG_MODULE,LOGIN_LOG_PROC,"redis:[ip = %s,port = %s]\n",redis_ip,redis_port);

    redis_conn = rop_connectdb_nopwd(redis_ip,redis_port);
    if(NULL == redis_conn){
        LOG(LOGIN_LOG_MODULE,LOGIN_LOG_PROC,"redis connect err\n");
        ret = -1;
        goto END;
    }

    //生成token

    //1.生成4个1000以内的随机数
    int rand_num[4] = {0};
    int i = 0;
    srand((unsigned int)time(NULL));
    for(i = 0;i < 4;++i){
        rand_num[i] = rand() % 1000;
    }

    //2.将四个随机数和用户名拼接在一起(token 的唯一性)
    char tmp[1024] = {0};
    sprintf(tmp,"%s%d%d%d%d",username,rand_num[0],rand_num[1],rand_num[2],rand_num[3]);
    LOG(LOGIN_LOG_MODULE,LOGIN_LOG_PROC,"tmp = %s\n",tmp);

    //3.对拼接出的字符串进行加密
    char enc_tmp[1024 * 2] = {0};
    int enc_len = 0;
    ret = DesEnc((unsigned char*)tmp,strlen(tmp),(unsigned char*)enc_tmp,&enc_len);
    if(ret != 0){
        LOG(LOGIN_LOG_MODULE,LOGIN_LOG_PROC,"DesEnc err\n");
        ret = -1;
        goto END;
    }

    //4.使用base64进行编码
    char base64[1024 * 3] = {0};
    base64_encode((const unsigned char*)enc_tmp,enc_len,base64);
    LOG(LOGIN_LOG_MODULE,LOGIN_LOG_PROC,"base64 = %s\n",base64);

    //5.base64转成md5
    MD5_CTX md5;
    MD5Init(&md5);
    unsigned char decrypt[16] = {0};
    MD5Update(&md5,(unsigned char*)base64,strlen(base64));
    MD5Final(&md5,decrypt);
    char str[100] = {0};
    for(i = 0;i < 16;++i){
        sprintf(str,"%02x",decrypt[i]);
        strcat(token,str);
    }

    //保存到redis中去，并且超时时间设置为24小时
    //key:username value:token
    ret = rop_setex_string(redis_conn,username,86400,token);

END:
    if(redis_conn != NULL){
        rop_disconnect(redis_conn);
    }

    return ret;
}

void return_login_status(char* status_num,char* token){
    char* out = NULL;
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root,"code",status_num);
    cJSON_AddStringToObject(root,"token",token);
    out = cJSON_Print(root);

    if(out != NULL){
        printf(out);
        free(out);
    }
}


int main(){
    while(FCGI_Accept() >= 0){
        char* contentLength = getenv("CONTENT_LENGTH");
        int len = 0;
        char token[128] = {0};

        printf("Content-type: text/html\r\n\r\n");

        if(contentLength == NULL){
            len = 0;
        }else{
            len = atoi(contentLength);
        }

        if(len <= 0){
            printf("No data from standard input.<p>\n");
            LOG(LOGIN_LOG_MODULE,LOGIN_LOG_PROC,"len = 0,No data from standard input\n");
        }else{
            char buf[4 * 1024] = {0};
            int ret = 0;
            ret = fread(buf,1,len,stdin);  //读出body
            if(ret == 0){
                LOG(LOGIN_LOG_MODULE,LOGIN_LOG_PROC,"fread(buf,1,len,sydin) return 0");
                continue;
            }

            LOG(LOGIN_LOG_MODULE,LOGIN_LOG_PROC,"buf = %s\n",buf);


            char username[512] = {0};
            char pwd[512] = {0};
            get_login_info(buf,username,pwd);
            LOG(LOGIN_LOG_MODULE,LOGIN_LOG_PROC,"username = %s,pwd = %s\n",username,pwd);

            ret = check_user_pwd(username,pwd);
            if(ret == 0){
                //符合条件的话
                memset(token,0,strlen(token));
                ret = set_token(username,token);
                LOG(LOGIN_LOG_MODULE,LOGIN_LOG_PROC,"token = %s\n",token);
            }


            if(ret == 0){
                return_login_status("000",token);
            }else{
                return_login_status("001","fail");
            }

        }
    }
    return 0;
}