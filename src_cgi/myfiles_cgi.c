/*
   给用户展示文件的cgi程序
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "make_log.h"
#include "util_cgi.h"
#include "deal_mysql.h"
#include "cfg.h"
#include "cJSON.h"

#include "fcgi_config.h"
#include "fcgi_stdio.h"

#define MYFILES_LOG_MODULE   "cgi"
#define MYFILES_LOG_PROC     "myfiles"

static char mysql_user[128] = {0};
static char mysql_pwd[128] = {0};
static char mysql_db[128] = {0};

void read_cfg(){
    get_cfg_value(CFG_PATH,"mysql","user",mysql_user);
    get_cfg_value(CFG_PATH,"mysql","password",mysql_pwd);
    get_cfg_value(CFG_PATH,"mysql","database",mysql_db);
    LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"mysql:[user = %s,pwd = %s,database = %s]\n",mysql_user,mysql_pwd,mysql_db);
}

int get_count_json_info(char* buf,char* user,char* token){
    int ret = 0;

    /*
       收到的json数据格式为
       {
           token:xxx,
           user:xxx
       } 
    */

   cJSON* root = cJSON_Parse(buf);
   if(NULL == root){
    LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"cJSON_Parse err\n");
    ret = -1;
    goto END;
   }

   cJSON* child1 = cJSON_GetObjectItem(root,"token");
   if(NULL == child1){
    LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"cJSON_GetObjectItem token err\n");
    ret = -1;
    goto END;
   }
   strcpy(token,child1->valuestring);

   cJSON* child2 = cJSON_GetObjectItem(root,"user");
   if(NULL == child2){
    LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"cJSON_GetObjectItem user err\n");
    ret = -1;
    goto END;
   }
   strcpy(user,child2->valuestring);

END:
   if(root != NULL){
    cJSON_Delete(root);
    root = NULL;
   }

   return ret;
}


void return_login_status(long num,int token_flag){
    char* out = NULL;
    char* token = NULL;
    char num_buf[128] = {0};

    if(token_flag == 0){
        token = "110";  //成功
    }else{
        token = "111";  //失败
    }
    sprintf(num_buf,"%ld",num);

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root,"num",num_buf);
    cJSON_AddStringToObject(root,"code",token);
    out = cJSON_Print(root);

    cJSON_Delete(root);
    if(out != NULL){
        printf(out);
        free(out);
    }
}

//获取用户文件个数
void get_user_files_count(char* user,int ret){
    char sql_cmd[SQL_MAX_LEN] = {0};
    MYSQL* conn = NULL;
    long line = 0;

    conn = mysql_conn(mysql_user,mysql_pwd,mysql_db);
    if(NULL == conn){
        LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"mysql_conn err\n");
        goto END;
    }

    mysql_query(conn,"set names utf8");
    
    sprintf(sql_cmd,"select count from user_file_count where user = '%s'",user);
    char tmp[512] = {0};

    int ret2 = process_result_one(conn,sql_cmd,tmp);
    if(ret2 != 0){
        LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"%s err\n",sql_cmd);
        goto END;
    }

    line = atoi(tmp); //该用户的文件数

END:
   if(conn != NULL){
    mysql_close(conn);
   }

   LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"line = %ld\n",line);

   return_login_status(line,ret);
}

//解析json包
int get_fileslist_json_info(char* buf,char* user,char* token,int* p_start,int* p_count){
    int ret = 0;

    /*
        {
            user:xxx
            token:xxx
            start:xxx
            count:xxx
        } 
    */

   cJSON* root = cJSON_Parse(buf);
   if(NULL == root){
     LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"cJSON_Parse err\n");
     ret = -1;
     goto END;
   }

   cJSON* child1 = cJSON_GetObjectItem(root,"user");
   if(NULL == child1){
    LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"cJSON_GetObjectItem err\n");
    ret = -1;
    goto END;
   }
   strcpy(user,child1->valuestring);

   cJSON* child2 = cJSON_GetObjectItem(root,"token");
   if(NULL == child2){
     LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"cJSON_GetObjectItem token err\n");
     ret = -1;
     goto END;
   }
   strcpy(token,child2->valuestring);

   cJSON* child3 = cJSON_GetObjectItem(root,"start");
   if(NULL == child3){
    LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"cJSON_GetObjectItem start err\n");
    ret = -1;
    goto END;
   }
   *p_start = child3->valueint;

   cJSON* child4 = cJSON_GetObjectItem(root,"count");
   if(NULL == child4){
    LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"cJSON_GetObjectItem count err\n");
    ret = -1;
    goto END;
   }
   *p_count = child4->valueint;

END:
   if(root != NULL){
    cJSON_Delete(root);
    root = NULL;
   }
   return ret;
}

//获取用户文件列表
/*
  获取用户文件信息  127.0.0.1:8000/myfiles&cmd=normal
  按下载量升序      127.0.0.1:8000/myfiles?cmd=pvasc
  按下载量降序      127.0.0.1:8000/myfiles?cmd=pvdesc
*/
int get_user_filelist(char* cmd,char* user,int start,int count){
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    MYSQL* conn = NULL;
    cJSON* root = NULL;
    cJSON* array = NULL;
    char* out = NULL;
    char* out2 = NULL;
    MYSQL_RES* result = NULL;

    conn = mysql_conn(mysql_user,mysql_pwd,mysql_db);
    if(NULL == conn){
        LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"mysql_conn err\n");
        ret = -1;
        goto END;
    }


    mysql_query(conn,"set names utf8");
    if(strcmp(cmd,"normal") == 0){
        //获取用户文件列表
        sprintf(sql_cmd,"select user_file_list.*,file_info.url,file_info.size,file_info.type from file_info,user_file_list where user = '%s' and file_info.md5 = user_file_list.md5 limit %d,%d",user,start,count);
    }else if(strcmp(cmd,"pvasc") == 0){
        //按下载量升序排列
        sprintf(sql_cmd,"select user_file_list.*,file_info.url,file_info.size,file_info.type from file_info,user_file_list where user = '%s' and file_info.md5 = user_file_list.md5 order by pv asc limit %d,%d",user,start,count);
    }else{
        sprintf(sql_cmd,"select user_file_list.*,file_info.url,file_info.size,file_info.type from file_info,user_file_list where user = '%s' and file_info.md5 = user_file_list.md5 order by pv desc limit %d,%d",user,start,count);
    }

    LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"%s will be execute\n");

    if(mysql_query(conn,sql_cmd) != 0){
        LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"%s err\n",sql_cmd);
        ret = -1;
        goto END;
    }
    result = mysql_store_result(conn);

    if(NULL == result){
        LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"mysql_store_result err,err = %s\n",mysql_error(conn));
        ret = -1;
        goto END;
    }

    ulong line = 0;
    line = mysql_num_rows(result); //得到记录的行数
    if(line == 0){
        LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"mysql_num_rows err,err = %s\n",mysql_error(conn));
        ret = -1;
        goto END;
    }
    MYSQL_ROW row;

    root = cJSON_CreateObject();
    array = cJSON_CreateArray();

    //下面就是对得到的结果进行提取 

    while((row = mysql_fetch_row(result)) != NULL){
        cJSON* item = cJSON_CreateObject();
        int col = 1;
        //文件所属用户
        if(row[col] != NULL){
            cJSON_AddStringToObject(item,"user",row[col]);
        }

        col++;
        //文件md5
        if(row[col] != NULL){
            cJSON_AddStringToObject(item,"md5",row[col]);
        }

        col++;
        if(row[col] != NULL){
            cJSON_AddStringToObject(item,"create_time",row[col]);
        }

        col++;
        if(row[col] != NULL){
            cJSON_AddStringToObject(item,"file_name",row[col]);
        }

        col++;
        if(row[col] != NULL){
            cJSON_AddNumberToObject(item,"share_status",atoi(row[col]));
        }

        col++;
        if(row[col] != NULL){
            cJSON_AddNumberToObject(item,"pv",atol(row[col]));
        }

        col++;
        if(row[col] != NULL){
            cJSON_AddStringToObject(item,"url",row[col]);
        }

        col++;
        if(row[col] != NULL){
            cJSON_AddNumberToObject(item,"size",atol(row[col]));
        }

        col++;
        if(row[col] != NULL){
            cJSON_AddStringToObject(item,"type",row[col]);
        }
        cJSON_AddItemToArray(array,item);
    }
    cJSON_AddItemToObject(root,"files",array);
    out = cJSON_Print(root);

    LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"%s\n",out);

END:
    if(ret == 0){
        printf("%s",out);  //向前端进行输出
    }else{
        //失败
        out2 = NULL;
        out2 = return_status("015");
    }

    if(out2 != NULL){
        printf(out2);
        free(out2);
    }

    if(result != NULL){
        mysql_free_result(result);
    }

    if(conn != NULL){
        mysql_close(conn);
    }

    if(root != NULL){
        cJSON_Delete(root);
    }

    if(out != NULL){
        free(out);
    }

    return ret;
}

int main(){
    char cmd[20] = {0};
    char user[USER_NAME_LEN] = {0};
    char token[TOKEN_LEN] = {0};

    read_cfg();

    while(FCGI_Accept() >= 0){
        // URL ? 之后的内容
        char* query = getenv("QUERY_STRING");
        LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"queyr_string = %s\n",query);

        query_parse_key_value(query,"cmd",cmd,NULL);
        LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"cmd = %s\n",cmd);

        char* contentLength = getenv("CONTENT_LENGTH");
        int len = 0;

        printf("Content-type: text/html\r\n\r\n");

        if(contentLength == NULL){
            len = 0;
        }else{
            len = atol(contentLength);
        }

        if(len <= 0){
            printf("No data from standard input\n");
            LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"len = 0,No data from stdandard input\n");
        }else{
            char buf[4 * 1024] = {0};
            int ret = 0;
            ret = fread(buf,1,len,stdin);

            if(ret == 0){
                LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"fread(buf,1,len,stdin) err\n");
                continue;
            }else{
                LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"buf = %s\n",buf);

                if(strcmp(cmd,"count") == 0){
                    get_count_json_info(buf,user,token);

                    ret = verify_token(user,token);

                    get_user_files_count(user,ret);
                }else{
                    int start = 0;
                    int count = 0;
                    get_fileslist_json_info(buf,user,token,&start,&count);
                    LOG(MYFILES_LOG_MODULE,MYFILES_LOG_PROC,"user =%s,token = %s,start = %d,count = %d\n",
                                                             user,token,start,count);
                    ret = verify_token(user,token);
                    if(ret == 0){
                        get_user_filelist(cmd,user,start,count);
                    }else{
                        char* out = return_status("111");
                        if(out != NULL){
                            printf(out);
                            free(out);
                        }
                    }
                }
            }
        }
    }
    return 0;
}