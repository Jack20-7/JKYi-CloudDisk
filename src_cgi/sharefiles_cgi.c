/*
    展示共享文件列表的CGI程序
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "make_log.h"
#include "util_cgi.h"
#include "deal_mysql.h"
#include "redis_keys.h"
#include "redis_op.h"
#include "cfg.h"
#include "cJSON.h"

#include "fcgi_config.h"
#include "fcgi_stdio.h"

#define SHAREFILES_LOG_MODULE   "cgi"
#define SHAREFILES_LOG_PROC     "sharefiles"

static char mysql_user[128] = {0};
static char mysql_pwd[128] = {0};
static char mysql_db[128] = {0};

static char redis_ip[30] = {0};
static char redis_port[10] = {0};

void read_cfg(){
    get_cfg_value(CFG_PATH,"mysql","user",mysql_user);
    get_cfg_value(CFG_PATH,"mysql","password",mysql_pwd);
    get_cfg_value(CFG_PATH,"mysql","database",mysql_db);
    LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"mysql:[user = %s,pwd = %s,database = %s\n]",mysql_user,mysql_pwd,mysql_db);

    get_cfg_value(CFG_PATH,"redis","ip",redis_ip); 
    get_cfg_value(CFG_PATH,"redis","port",redis_port);
    LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"redis:[ip = %s,port = %s\n]",redis_ip,redis_port);
}

//获取共享文件的个数
void get_share_files_count(){
    char sql_cmd[SQL_MAX_LEN] = {0};
    MYSQL* conn = NULL;
    long line = 0;

    conn = mysql_conn(mysql_user,mysql_pwd,mysql_db);
    if(NULL == conn){
        LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"mysql_conn err\n");
        goto END;
    }

    sprintf(sql_cmd,"select count from user_file_count where user = '%s'","xxx_share_xxx_file_xxx_list_xxx_count_xxx");
    char tmp[512] = {0};
    int ret2 = process_result_one(conn,sql_cmd,tmp);
    if(ret2 != 0){
        LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"%s err\n",sql_cmd);
        goto END;
    }
    line = atol(tmp);

END:
   if(conn != NULL){
    mysql_close(conn);
   }

   LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"share_file_count = %ld\n",line);
   printf("%ld",line);

}

int get_fileslist_json_info(char* buf,int* p_start,int* p_count){
    int ret = 0;
    //收到的json数据格式为
    /*
       {
           "start":0,
           "count":10
       }
    */

   cJSON* root = cJSON_Parse(buf);
   if(NULL == root){
     LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"cJSON_Parse err\n");
     ret = -1;
     goto END;
   }

   cJSON* child1 = cJSON_GetObjectItem(root,"start");
   if(NULL == child1){
     LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"cJSON_GetObjectItem start err\n");
     ret = -1;
     goto END;
   }
   *p_start = child1->valueint;

   cJSON* child2 = cJSON_GetObjectItem(root,"count");
   if(NULL == child2){
     LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"cJSON_GetObjectItem count err\n");
     ret = -1;
     goto END;
   }
   *p_count = child2->valueint;

END:
   if(root != NULL){
    cJSON_Delete(root);
    root = NULL;
   }

   return ret;
}

//http://127.0.0.1:8000/sharefiles?cmd=normal
int get_share_filelist(int start,int count){
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    MYSQL* conn = NULL;
    cJSON* root = NULL;
    cJSON* array = NULL;
    char* out = NULL;
    char* out2 = NULL;
    MYSQL_RES* result = NULL;
    MYSQL_ROW row;

    conn = mysql_conn(mysql_user,mysql_pwd,mysql_db);
    if(NULL == conn){
        LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"mysql conn err\n");
        goto END;
    }
    mysql_query(conn,"set names utf8");

    sprintf(sql_cmd,"select share_file_list.*,file_info.url,file_info.size,file_info.type from file_info,share_file_list where file_info.md5 = share_file_list.md5 limit %d,%d",start,count);
    LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"%s execute\n",sql_cmd);

    if(mysql_query(conn,sql_cmd) != 0){
        LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"%s err\n",sql_cmd);
        ret = -1;
        goto END;
    }

    result = mysql_store_result(conn);  //获取结果集
    if(NULL == result){
        LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"mysql_store_result err\n");
        ret = -1;
        goto END;
    }

    ulong line = 0;
    line = mysql_num_rows(result);  //获取结果集中记录的条数
    if(line == 0){
        LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"mysql_num_rows ret 0\n");
        ret = -1;
        goto END;
    }

    root = cJSON_CreateObject();
    array = cJSON_CreateArray();
    while((row = mysql_fetch_row(result)) != NULL){
        //取出每一条记录
        cJSON* item = cJSON_CreateObject();
        int col = 0;

        if(row[col] != NULL){
            cJSON_AddStringToObject(item,"user",row[col]);
        }

        col++;
        if(row[col] != NULL){
            cJSON_AddStringToObject(item,"md5",row[col]);
        }

        col++;
        if(row[col] != NULL){
            cJSON_AddStringToObject(item,"file_name",row[col]);
        }

        col++;
        if(row[col] != NULL){
            cJSON_AddNumberToObject(item,"share_status",1);
        }

        col++;
        if(row[col] != NULL){
            cJSON_AddNumberToObject(item,"pv",atol(row[col]));
        }

        col++;
        if(row[col] != NULL){
            cJSON_AddStringToObject(item,"create_time",row[col]);
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
    LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"%s\n",out);
END:
    if(ret == 0){
        printf("%s",out);
    }else{
        out2 = NULL;
        out2 = return_status("015");
    }
    if(out2 != NULL){
        printf("%s",out2);
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

//获取共享文件排行榜，按下载次数降序排列
//http://127.0.0.1:8000/sharefiles?cmd=pvdesc

int get_ranking_filelist(int start,int count){
    /*
       1.mysql共享文件数和redis中共享文件数对比，判断是否相同 
          相同:直接从redis中读取数据返回给前端
          不相同:删除掉redis中的数据，重新从mysql中进行读取
    */

   int ret = 0;
   char sql_cmd[SQL_MAX_LEN] = {0};
   MYSQL* conn = NULL;
   cJSON* root = NULL;
   cJSON* array = NULL;
   RVALUES value = NULL;
   char* out = NULL;
   char* out2 = NULL;
   char tmp[512] = {0};
   int ret2 = 0;
   MYSQL_RES* result = NULL;
   redisContext* redis_conn = NULL;

   redis_conn = rop_connectdb_nopwd(redis_ip,redis_port);
   if(NULL == redis_conn){
    LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"rop_connectdb_nopwd err\n");
    ret = -1;
    goto END;
   }

   conn = mysql_conn(mysql_user,mysql_pwd,mysql_db);
   if(NULL == conn){
    LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"mysql_conn err\n");
    ret = -1;
    goto END;
   }
   mysql_query(conn,"set names utf8");

   //查询mysql中共享文件的数量
   sprintf(sql_cmd,"select count from user_file_count where user = '%s'","xxx_share_xxx_file_xxx_list_xxx_count_xxx");
   ret2 = process_result_one(conn,sql_cmd,tmp);
   if(ret2 != 0){
     LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"%s err\n",sql_cmd);
     ret = -1;
     goto END;
   }
   int sql_num = atoi(tmp);

   int redis_num = rop_zset_zcard(redis_conn,FILE_PUBLIC_ZSET); //redis中zset集合元素的个数
   if(redis_num == -1){
    LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"rop_zset_zcard err\n");
    ret = -1;
    goto END;
   }

   LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"sql_num = %d,redis_num = %d\n",sql_num,redis_num);
   //判断是否相同
   if(sql_num != redis_num){
    //如果不相同的话
    
    //1.清空redis zset容器中元素的个数
    rop_del_key(redis_conn,FILE_PUBLIC_ZSET);
    rop_del_key(redis_conn,FILE_NAME_HASH);

    //2.重新从mysql中读出数据到redis
    strcpy(sql_cmd,"select md5,file_name,pv from share_file_list order by pv desc");
    LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"%s will be execute\n",sql_cmd);
    if(mysql_query(conn,sql_cmd) != 0){
        LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"%s err\n",sql_cmd);
        ret = -1;
        goto END;
    }
    result = mysql_store_result(conn);
    if(NULL == result){
        LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"mysql_store_result err\n");
        ret = -1;
        goto END;
    }

    ulong line = 0;
    line = mysql_num_rows(result);
    if(line == 0){
        LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"mysql_num_rows ret 0\n");
        ret = -1;
        goto END;
    }

    MYSQL_ROW row;
    while((row = mysql_fetch_row(result)) != NULL){
        if(row[0] == NULL || row[1] == NULL || row[2] == NULL){
            LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"mysql_fetch_row err\n");
            ret = -1;
            goto END;
        }

        char fileid[1024] = {0};
        sprintf(fileid,"%s%s",row[0],row[1]); //md5 + filename
        //score- pv
        //name- fieldid
        rop_zset_add(redis_conn,FILE_PUBLIC_ZSET,atoi(row[2]),fileid);
        //key- fieldid
        //value- filename
        rop_hash_set(redis_conn,FILE_NAME_HASH,fileid,row[1]);
    }
   }

   //从redis中读取数据，返回给前端
   value = (RVALUES)calloc(count,VALUES_ID_SIZE);
   if(NULL == value){
    ret = -1;
    goto END;
   }

   int n = 0;
   int end = start + count - 1;
   ret = rop_zset_zrevrange(redis_conn,FILE_PUBLIC_ZSET,start,end,value,&n);
   if(ret != 0){
    LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"rop_zset_zrevrange err\n");
    goto END;
   }

   root = cJSON_CreateObject();
   array = cJSON_CreateArray();
   for(int i = 0;i < n;++i){
     cJSON* item = cJSON_CreateObject();

     /*
        //需要返回给前端的格式
        {
             "filename":"XXX",
             "pv":0
        }
     */

    char filename[1024] = {0};
    ret = rop_hash_get(redis_conn,FILE_NAME_HASH,value[i],filename);
    if(ret != 0){
        LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"rop_hash_get %s err\n",value[i]);
        ret = -1;
        goto END;
    }
    cJSON_AddStringToObject(item,"filename",filename);

    int score = rop_zset_get_score(redis_conn,FILE_PUBLIC_ZSET,value[i]);
    if(score == -1){
        LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"rop_zset_get_score err\n");
        ret = -1;
        goto END;
    }
    cJSON_AddNumberToObject(item,"pv",score);

    cJSON_AddItemToArray(array,item);
   }
   cJSON_AddItemToObject(root,"files",array);
   out = cJSON_Print(root);

   LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"%s\n",out);

END:
   if(ret == 0){
    printf("%s",out);
   }else{
    out2 = NULL;
    out2 = return_status("015");
   }
   if(out2 != NULL){
    printf(out2);
    free(out2);
   }
   if(NULL != result){
    mysql_free_result(result);
   }

   if(redis_conn != NULL){
    rop_disconnect(redis_conn);
   }
   if(conn != NULL){
    mysql_close(conn);
   }
   if(value != NULL){
     free(value);
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
    read_cfg();

    while(FCGI_Accept() >= 0){
        char* query = getenv("QUERY_STRING");

        query_parse_key_value(query,"cmd",cmd,NULL);
        LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"cmd = %s\n",cmd);

        printf("Content-type: text/html\r\n\r\n");
        if(strcmp(cmd,"count") == 0){
            get_share_files_count();
        }else{
            char* contentLength = getenv("CONTENT_LENGTH");
            int len = 0;

            if(contentLength == NULL){
                len = 0;
            }else{
                len = atoi(contentLength);
            }

            if(len <= 0){
                printf("No data from stanard input.\n");
                LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"len = 0,No data from standard input\n");
            }else{
                char buf[4 * 1024] = {0};
                int ret = 0;
                ret = fread(buf,1,len,stdin);
                if(ret == 0){
                    LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"fread(buf,1,len,stdin) err\n");
                    continue;
                }

                LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"buf = %s\n",buf);

                //127.0.0.1:8000/sharefiles?cmd=normal
                //127.0.0.1:8000/sharefiles?cmd=pvdesc

                int start = 0;
                int count = 0;
                get_fileslist_json_info(buf,&start,&count);
                LOG(SHAREFILES_LOG_MODULE,SHAREFILES_LOG_PROC,"start = %d,count = %d\n",start,count);
                if(strcmp(cmd,"normal") == 0){
                    get_share_filelist(start,count);
                }else if(strcmp(cmd,"pvdesc") == 0){
                    get_ranking_filelist(start,count);
                }
            }
        }
    }
    return 0;
}



