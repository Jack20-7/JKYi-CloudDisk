/*
  秒传功能的cgi
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "make_log.h"
#include "deal_mysql.h"
#include "util_cgi.h"
#include "cfg.h"
#include "cJSON.h"

#include "fcgi_config.h"
#include "fcgi_stdio.h"

#define MD5_LOG_MODULE "cgi"
#define MD5_LOG_PROC   "md5"


static char mysql_user[128] = {0};
static char mysql_pwd[128] = {0};
static char mysql_db[128] = {0};

void read_cfg(){
    get_cfg_value(CFG_PATH,"mysql","user",mysql_user);
    get_cfg_value(CFG_PATH,"mysql","password",mysql_pwd);
    get_cfg_value(CFG_PATH,"mysql","database",mysql_db);
    LOG(MD5_LOG_MODULE,MD5_LOG_PROC,"mysql:[user = %s,pwd = %s,database = %s]\n",mysql_user,mysql_pwd,mysql_db);
}

//解析从客户端发送来的json报文
int get_md5_info(char* buf,char* user,char* token,char* md5,char* filename){
    int ret = 0;

    /*
       {
          user:xxx,
          token:xxx,
          md5:xxx,
          fileName:xxx
       } 
    */
    cJSON* root = cJSON_Parse(buf);
    if(NULL == root){
        LOG(MD5_LOG_MODULE,MD5_LOG_PROC,"cJSON_Parse err\n");
        ret = -1;
        goto END;
    }

    cJSON* child1 = cJSON_GetObjectItem(root,"user");
    if(NULL == child1){
        LOG(MD5_LOG_MODULE,MD5_LOG_PROC,"cJSON_GetobjectItem user err\n");
        ret = -1;
        goto END;
    }
    strcpy(user,child1->valuestring);

    cJSON* child2 = cJSON_GetObjectItem(root,"md5");
    if(NULL == child2){
        LOG(MD5_LOG_MODULE,MD5_LOG_PROC,"cJSON_GetobjectItem md5 err\n");
        ret = -1;
        goto END;
    }
    strcpy(md5,child2->valuestring);

    cJSON* child3 = cJSON_GetObjectItem(root,"fileName");
    if(NULL == child3){
        LOG(MD5_LOG_MODULE,MD5_LOG_PROC,"cJSON_GetObjectItem fileName err\n");
        ret = -1;
        goto END;
    }
    strcpy(filename,child3->valuestring);

    cJSON* child4 = cJSON_GetObjectItem(root,"token");
    if(NULL == child4){
        LOG(MD5_LOG_MODULE,MD5_LOG_PROC,"cJSON_GetObjectItem token err\n");
        ret = -1;
        goto END;
    }
    strcpy(token,child4->valuestring);

END:
    if(root != NULL){
        cJSON_Delete(root);
        root = NULL;
    }
    return ret;
}

/// @brief 秒传处理
/// @param user 
/// @param md5 
/// @param filename 
/// @return  0:秒传成功  -1:出错 -2:该用户已拥有此文件  -3:秒传失败
int deal_md5(char* user,char* md5,char* filename){
    int ret = 0;
    MYSQL* conn = NULL;
    int ret2 = 0;
    char tmp[512] = {0};
    char sql_cmd[SQL_MAX_LEN] = {0};
    char* out = NULL;


    conn = mysql_conn(mysql_user,mysql_pwd,mysql_db);
    if(NULL == conn){
        LOG(MD5_LOG_MODULE,MD5_LOG_PROC,"mysql conn err\n");
        ret = -1;
        goto END;
    }

    mysql_query(conn,"set names utf8");

    //根据md5值到file_info表中查找是否存在相同文件
    sprintf(sql_cmd,"select count from file_info where md5 = '%s'",md5);
    ret2 = process_result_one(conn,sql_cmd,tmp);
    //process_result_one返回值:
    //0:成功并获取到的值
    //1:没有结果返回
    //2.有结果但是为传入buf
    //-1.失败
    if(ret2 == 0){
        int count = atoi(tmp);
        //查看该用户是否存在此文件,如果存在的话，就无需在上传
        sprintf(sql_cmd,"select * from user_file_list where user = '%s' and md5 = '%s' and file_name = '%s'",user,md5,filename);
        ret2 = process_result_one(conn,sql_cmd,NULL);
        if(ret2 == 2){
            //该用于已经有了文件
            LOG(MD5_LOG_MODULE,MD5_LOG_PROC,"%s[filename:%s,md5:%s] is exists\n",user,filename,md5);
            ret = -2;
            goto END;
        }

        //如果用户没有此文件的话，就进行上传
        sprintf(sql_cmd,"update file_info set count = %d where md5 = '%s'",++count,md5);
        if(mysql_query(conn,sql_cmd) != 0){
            LOG(MD5_LOG_MODULE,MD5_LOG_PROC,"%s failed\n",sql_cmd);
            ret = -1;
            goto END;
        }

        struct timeval tv;
        struct tm* ptm;
        char time_str[128] = {0};

        gettimeofday(&tv,NULL);
        ptm = localtime(&tv.tv_sec);
        strftime(time_str,sizeof(time_str),"%Y-%m-%d %H:%M:%S",ptm);

        sprintf(sql_cmd,"insert into user_file_list(user,md5,create_time,file_name,shared_status,pv)"
                        "values('%s','%s','%s','%s',%d,%d)",user,md5,time_str,filename,0,0);
        if(mysql_query(conn,sql_cmd) != 0){
            LOG(MD5_LOG_MODULE,MD5_LOG_PROC,"%s failed,errstr = %s\n",sql_cmd,mysql_error(conn));
            ret = -1;
            goto END;
        }

        //查询用户的文件数
        sprintf(sql_cmd,"select count from user_file_count where user = '%s'",user);
        count = 0;

        ret2 = process_result_one(conn,sql_cmd,tmp);
        if(ret2 == 1){
            //没有记录的话
            sprintf(sql_cmd, " insert into user_file_count (user, count) values('%s', %d)", user, 1);
        }else{
            //更新用户文件数量count字段
            count = atoi(tmp);
            sprintf(sql_cmd, "update user_file_count set count = %d where user = '%s'", count+1, user);
        }

        if(mysql_query(conn,sql_cmd) != 0){
            LOG(MD5_LOG_MODULE,MD5_LOG_PROC,"%s failed,errstr = %s\n",mysql_error(conn));
            ret = -1;
            goto END;
        }
    }else if(ret2 == 1){
        //没有结果，秒传失败
        ret = -3;
        goto END;
    }

END:
    /*
       //返回的json串
       文件存在:{"code":"005"}
       秒传成功:{"code":"006"}
       秒传失败:{"code","007"}
    */

   if(ret == 0){
    out = return_status("006");
   }else if(ret == -2){
    out = return_status("005");
   }else{
    out = return_status("007");
   }

   if(out != NULL){
    printf(out);
    free(out);
   }


   if(conn != NULL){
    mysql_close(conn);
   }

   return ret;
}

int main(){
    read_cfg();

    while(FCGI_Accept() >= 0){
        char* contentLength = getenv("CONTENT_LENGTH");
        int len = 0;

        printf("Content-type: text/html\r\n\r\n");

        if(contentLength == NULL){
            len = 0;
        }else{
            len = atoi(contentLength);
        }

        if(len <= 0){
            printf("No data from standard input.<p>\n");
            LOG(MD5_LOG_MODULE, MD5_LOG_PROC, "len = 0, No data from standard input\n");
        }else{
            char buf[4 * 1024] = {0};
            int ret = 0;
            ret = fread(buf,1,len,stdin);
            if(ret == 0){
                LOG(MD5_LOG_MODULE,MD5_LOG_PROC,"fread(buf,1,len,stdin) err\n");
                continue;
            }

            LOG(MD5_LOG_MODULE,MD5_LOG_PROC,"buf = %s\n",buf);

            char user[128] = {0};
            char md5[256] = {0};
            char token[256] = {0};
            char filename[128] = {0};
            ret = get_md5_info(buf,user,token,md5,filename);

            if(ret != 0){
                LOG(MD5_LOG_MODULE,MD5_LOG_PROC,"get_md5_info err\n");
                continue;
            }
            LOG(MD5_LOG_MODULE,MD5_LOG_PROC,"user = %s,token = %s,md5 = %s,filename = %s\n",
                                            user,token,md5,filename);
            ret = verify_token(user,token);
            if(ret == 0){
                deal_md5(user,md5,filename);
            }else{
                char* out = return_status("111");
                if(out != NULL){
                    printf(out);
                    free(out);
                }
            }
        }
    }
    return 0;
}