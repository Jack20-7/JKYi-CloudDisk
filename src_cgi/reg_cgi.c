#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "fcgi_config.h"
#include "fcgi_stdio.h"

#include "make_log.h"
#include "util_cgi.h"
#include "deal_mysql.h"
#include "cfg.h"
#include "cJSON.h"

#define REG_LOG_MODULE "cgi"
#define REG_LOG_PROC   "reg"

int get_reg_info(char* reg_buf,char* user_name,char* nick_name,char* pwd,char* tel,char* email){
    int ret = 0;
    //客户端传过来的数据是json格式的
    /* 
      {
        userName:xx,
        nickName:xx,
        firstPwd:xx,
        phone:xx,
        email:xx
      }
    */

   cJSON* root = cJSON_Parse(reg_buf);
   if(NULL == root){
     LOG(REG_LOG_MODULE,REG_LOG_PROC,"cJSON_Parse err\n");
     ret = -1;
     goto END;
   }

   cJSON* child1 = cJSON_GetObjectItem(root,"userName");
   if(NULL == child1){
    LOG(REG_LOG_MODULE,REG_LOG_PROC,"userName\n");
    ret = -1;
    goto END;
   }
   strcpy(user_name,child1->valuestring);

   cJSON* child2 = cJSON_GetObjectItem(root,"nickName");
   if(NULL == child2){
    LOG(REG_LOG_MODULE,REG_LOG_PROC,"nickName\n");
    ret = -1;
    goto END;
   }
   strcpy(nick_name,child2->valuestring);

   cJSON* child3 = cJSON_GetObjectItem(root,"firstPwd");
   if(NULL == child3){
     LOG(REG_LOG_MODULE,REG_LOG_PROC,"firstPwd\n");
     ret = -1;
     goto END;
   }
    strcpy(pwd,child3->valuestring);

    cJSON* child4 = cJSON_GetObjectItem(root,"phone");
    if(NULL == child4){
        LOG(REG_LOG_MODULE,REG_LOG_PROC,"phone\n");
        ret = -1;
        goto END;
    }
    strcpy(tel,child4->valuestring);

    cJSON* child5 = cJSON_GetObjectItem(root,"email");
    if(NULL == child5){
        LOG(REG_LOG_MODULE,REG_LOG_PROC,"email\n");
        ret = -1;
        goto END;
    }
    strcpy(email,child5->valuestring);

END:
   if(root != NULL){
    cJSON_Delete(root);
    root = NULL;
   }
   return ret;
}

//注册操作
int user_register(char* reg_buf){
    int ret = 0;
    MYSQL* conn = NULL;

    char mysql_user[USER_NAME_LEN] = {0};
    char mysql_pwd[PWD_LEN] = {0};
    char mysql_db[256] = {0};
    if(get_cfg_value(CFG_PATH,"mysql","user",mysql_user) != 0){
        LOG(REG_LOG_MODULE,REG_LOG_PROC,"get mysql user\n");
        ret = -1;
        goto END;
    }
    if(get_cfg_value(CFG_PATH,"mysql","password",mysql_pwd) != 0){
        LOG(REG_LOG_MODULE,REG_LOG_PROC,"get mysql password\n");
        ret = -1;
        goto END;
    }
    if(get_cfg_value(CFG_PATH,"mysql","database",mysql_db) != 0){
        LOG(REG_LOG_MODULE,REG_LOG_PROC,"get mysql database\n");
        ret = -1;
        goto END;
    }
    LOG(REG_LOG_MODULE,REG_LOG_PROC,"mysql_user = %s,mysql_pwd = %s,mysql_db = %s\n",mysql_user,mysql_pwd,mysql_db);

    char user_name[128] = {0};
    char nick_name[128] = {0};
    char pwd[128] = {0};
    char tel[128] = {0};
    char email[128] = {0};

    ret = get_reg_info(reg_buf,user_name,nick_name,pwd,tel,email);
    if(ret != 0){
        goto END;
    }
    LOG(REG_LOG_MODULE,REG_LOG_PROC,"user_name = %s,nick_name = %s,pwd = %s,tel = %s,email = %s\n",user_name,nick_name,pwd,tel,email);

    conn = mysql_conn(mysql_user,mysql_pwd,mysql_db);
    if(NULL == conn){
        LOG(REG_LOG_MODULE,REG_LOG_PROC,"reg connect mysql err\n");
        ret = -1;
        goto END;
    }

    //设置编码方式
    mysql_query(conn,"set names utf8");

    char sql_cmd[SQL_MAX_LEN] = {0};
    sprintf(sql_cmd,"select * from user_info where user_name = '%s'",user_name);

    //下面就是通过sql语句判断该用户是否存在
    int ret2 = 0;
    ret2 = process_result_one(conn,sql_cmd,NULL);
    if(ret2 == -2){
        //用户已经存在
        LOG(REG_LOG_MODULE,REG_LOG_PROC,"user:%s is exists!\n");
        ret = -2;
        goto END;
    }

    //该用户还不存在,就需要创建
    struct timeval tv;
    struct tm* ptm = NULL;
    char time_str[128] = {0};

    gettimeofday(&tv,NULL);      //获取从1970-1-1到当前时间的偏移量，精确到微秒(us)
    ptm = localtime(&tv.tv_sec); //偏移量转化为本地时间
    //strftime函数的作用就是根据区域设置格式化本地时间，所以这里就是格式化出一个时间字符串
    strftime(time_str,sizeof(time_str),"%Y-%m-%d %H:%M:%S",ptm);

    //向mysql中插入一条记录
    sprintf(sql_cmd,"insert into user_info(user_name,nick_name,password,phone,email,create_time) "
                     "values ('%s','%s','%s','%s','%s','%s')",
                             user_name,nick_name,pwd,tel,email,time_str);
   if(mysql_query(conn,sql_cmd) != 0){
    LOG(REG_LOG_MODULE,REG_LOG_PROC,"%s excute err:%s\n",sql_cmd,mysql_error(conn));
    ret = -1;
    goto END;
   }

END:
   if(conn != NULL){
    mysql_close(conn);
   }

   return ret;
}

int main(){
    while(FCGI_Accept() >= 0){
        //阻塞等待用户连接
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
            LOG(REG_LOG_MODULE,REG_LOG_PROC,"len = 0,No data from standard input\n");
        }else{
            char buf[4 * 1024] = {0};
            int ret = 0;
            char* out = NULL;
            ret = fread(buf,1,len,stdin); //读取收到的http报文，读取到buf中
            if(ret == 0){
                LOG(REG_LOG_MODULE,REG_LOG_PROC,"fread(buf,1,len,stdin) err\n");
                continue;
            }

            LOG(REG_LOG_MODULE,REG_LOG_PROC,"buf = %s\n",buf);

            //下面就是注册用户的操作
            /*
               success 0
               fail   -1
               exist  -2

               返回的http响应报文报文体中携带的json串内容
               成功:     {"code","002"}
               存在      {"code","003"}
               失败      {"code","004"}
            */

           ret = user_register(buf);
           if(ret == 0){
            out = return_status("002");
           }else if(ret == -1){
            out = return_status("004");
           }else{
            out = return_status("003");
           }
           if(out != NULL){
            printf(out);
            free(out);
           }
        }
    }
    return 0;
}
