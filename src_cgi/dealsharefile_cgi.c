/*
   共享文件pv字段处理、取消分享、转存cgi程序
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
#include "redis_op.h"
#include "redis_keys.h"

#include "fcgi_stdio.h"
#include "fcgi_config.h"

#define DEALSHAREFILE_LOG_MODULE  "cgi"
#define DEALSHAREFILE_LOG_PROC    "dealsharefile"

static char mysql_user[128] = {0};
static char mysql_pwd[128] = {0};
static char mysql_db[128] = {0};

static char redis_ip[30] = {0};
static char redis_port[10] = {0};

void read_cfg(){
    //读取mysql数据库配置信息
    get_cfg_value(CFG_PATH, "mysql", "user", mysql_user);
    get_cfg_value(CFG_PATH, "mysql", "password", mysql_pwd);
    get_cfg_value(CFG_PATH, "mysql", "database", mysql_db);
    LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "mysql:[user=%s,pwd=%s,database=%s]", mysql_user, mysql_pwd, mysql_db);

    //读取redis配置信息
    get_cfg_value(CFG_PATH, "redis", "ip", redis_ip);
    get_cfg_value(CFG_PATH, "redis", "port", redis_port);
    LOG(DEALSHAREFILE_LOG_MODULE, DEALSHAREFILE_LOG_PROC, "redis:[ip=%s,port=%s]\n", redis_ip, redis_port);
}

//解析从客户端发送来的json报文
int get_json_info(char* buf,char* user,char* md5,char* filename){
    int ret = 0;
    /*
       //收到的json报文的格式为
       {
           "user":"nn",
           "md5":"xx",
           "filename":"xxx"
       } 
    */

   cJSON* root = cJSON_Parse(buf);
   if(NULL == root){
     LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"cJSON_Parse err\n");
     ret = -1;
     goto END;
   }

   cJSON* child1 = cJSON_GetObjectItem(root,"user");
   if(NULL == child1){
     LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"cJSON_GetObjectItem user err\n");
     ret = -1;
     goto END;
   }
   strcpy(user,child1->valuestring);

   cJSON* child2 = cJSON_GetObjectItem(root,"md5");
   if(NULL == child2){
    LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"cJSON_GetObjectItem md5 err\n");
    ret = -1;
    goto END;
   }
   strcpy(md5,child2->valuestring);

   cJSON* child3 = cJSON_GetObjectItem(root,"filename");
   if(NULL == child3){
    LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"cJSON_GetObjectItem filename err\n");
    ret = -1;
    goto END;
   }
   strcpy(filename,child3->valuestring);

END:
   if(root != NULL){
     cJSON_Delete(root);
     root = NULL;
   }

   return ret;
}

//文件下载标志处理
int pv_file(char* md5,char* filename){
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    MYSQL* conn = NULL;
    char* out = NULL;
    char tmp[512] = {0};
    int ret2 = 0;
    redisContext* redis_conn = NULL;
    char fileid[1024] = {0};

    redis_conn = rop_connectdb_nopwd(redis_ip,redis_port);
    if(NULL == redis_conn){
        LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"redis_conn err\n");
        ret = -1;
        goto END;
    }
    sprintf(fileid,"%s%s",md5,filename);

    conn = mysql_conn(mysql_user,mysql_pwd,mysql_db);
    if(NULL == conn){
        LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"mysql_conn err\n");
        ret = -1;
        goto END;
    }
    mysql_query(conn,"set names utf8");

    //1.将mysql中的pv字段 + 1
    sprintf(sql_cmd,"select pv from share_file_list where file_name = '%s' and md5 = '%s'",filename,md5);
    LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"sql:%s\n",sql_cmd);

    ret2 = process_result_one(conn,sql_cmd,tmp);
    int pv = 0;
    if(ret2 == 0){
        pv = atoi(tmp);
    }else{
        LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"%s err\n",sql_cmd);
        ret = -1;
        goto END;
    }
    sprintf(sql_cmd,"update share_file_list set pv = %d where md5 = '%s' and file_name = '%s'",pv + 1,md5,filename);
    LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"sql:%s\n",sql_cmd);
    if(mysql_query(conn,sql_cmd) != 0){
        LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"%s err\n",sql_cmd);
        ret = -1;
        goto END;
    }

    //2.判断元素是否还在redis中，不存在就从mysql中读入，存在就更新
     ret2 = rop_zset_exit(redis_conn,FILE_PUBLIC_ZSET,fileid);
    if(ret2 == 1){
        //存在
        //那么就需要对redis中的pv进行更新
        ret = rop_zset_increment(redis_conn,FILE_PUBLIC_ZSET,fileid);
        if(ret != 0){
            LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"rop_zset_increment err\n");
        }
    }else if(ret2 == 0){
        //不存在
        //就需要重新对数据进行读取
        rop_zset_add(redis_conn,FILE_PUBLIC_ZSET,pv + 1,fileid);
        rop_hash_set(redis_conn,FILE_NAME_HASH,fileid,filename);
    }else{
        //出现错误
        ret = -1;
        goto END;
    }

END:
    /*
       返回的json串格式为:
        成功: {"code":"016"}
        失败: {"code","017"}
    */

   out = NULL;
   if(ret == NULL){
    out = return_status("016");
   }else{
    out = return_status("017");
   }

   if(out != NULL){
    printf(out);
    free(out);
   }

   if(redis_conn != NULL){
    rop_disconnect(redis_conn);
   }

   if(conn != NULL){
    mysql_close(conn);
   }

   return ret;
}

//取消分享文件
int cancel_share_file(char* user,char* md5,char* filename){
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    MYSQL* conn = NULL;
    redisContext* redis_conn = NULL;
    char* out = NULL;
    char fileid[1024] = {0};

    redis_conn = rop_connectdb_nopwd(redis_ip,redis_port);
    if(NULL == redis_conn){
        LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"redis connect err\n");
        ret = -1;
        goto END;
    }

    conn = mysql_conn(mysql_user,mysql_pwd,mysql_db);
    if(NULL == conn){
        LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"mysql_conn err\n");
        ret = -1;
        goto END;
    }
    mysql_query(conn,"set names utf8");

    sprintf(fileid,"%s%s",md5,filename);

    //先是对mysql中的状态进行更新
    sprintf(sql_cmd,"update user_file_list set shared_status = 0 where user = '%s' and md5 = '%s' and file_name = '%s'",user,md5,filename);

    if(mysql_query(conn,sql_cmd) != 0){
        LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"%s err\n",sql_cmd);
        ret = -1;
        goto END;
    }
    sprintf(sql_cmd,"select count from user_file_count where user = '%s'","xxx_share_xxx_file_xxx_list_xxx_count_xxx");
    int count = 0;
    char tmp[512] = {0};
    int ret2 = process_result_one(conn,sql_cmd,tmp);
    if(ret2 != 0){
        LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"%s err\n",sql_cmd);
        ret = -1;
        goto END;
    }
    count = atoi(tmp);
    if(count == 1){
        sprintf(sql_cmd,"delete from user_file_count where user = '%s'","xxx_share_xxx_file_xxx_list_xxx_count_xxx");
    }else{
        sprintf(sql_cmd,"update user_file_count set count = %s where user = '%s'",count - 1,"xxx_share_xxx_file_xxx_list_xxx_count_xxx");
    }
    if(mysql_query(conn,sql_cmd) != 0){
        LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"%s err\n",sql_cmd);
        ret = -1;
        goto END;
    }
    sprintf(sql_cmd,"delete from share_file_list where user = '%s' and md5 = '%s' and file_name = '%s'",user,md5,filename);
    if(mysql_query(conn,sql_cmd) != 0){
        LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"%s err\n",sql_cmd);
        ret = -1;
        goto END;
    }

    //对redis进行更新
    ret = rop_zset_zrem(redis_conn,FILE_PUBLIC_ZSET,fileid);
    if(ret != 0){
        LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"rop_zset_zrem err\n");
        goto END;
    }

    ret = rop_hash_del(redis_conn,FILE_NAME_HASH,fileid);
    if(ret != 0){
        LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"rop_hash_del err\n");
        goto END;
    }
    
END:
   /*
      成功:{"code","018"} 
      失败:{"code",019}
   */
  out = NULL;
  if(ret == 0){
    out = return_status("018");
  }else{
    out = return_status("019");
  }
  if(out != NULL){
    printf(out);
    free(out);
  }
  if(redis_conn != NULL){
    rop_disconnect(redis_conn);
  }

  if(conn != NULL){
    mysql_close(conn);
  }

  return ret;
}

//转存文件
//0-成功   -1-失败  -2 文件以存在
int save_file(char* user,char* md5,char* filename){
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    MYSQL* conn = NULL;
    char* out = NULL;
    int ret2 = 0;
    char tmp[512] = {0};

    conn = mysql_conn(mysql_user,mysql_pwd,mysql_db);
    if(NULL == conn){
        LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"mysql_conn err\n");
        ret = -1;
        goto END;
    }
    mysql_query(conn,"set names utf8");

    sprintf(sql_cmd,"select * from user_file_list where user = '%s' and md5 = '%s' and file_name = '%s'",user,md5,filename);
    ret2 = process_result_one(conn,sql_cmd,NULL);
    if(ret2 == 2){
        //表示该用户已经有了此文件
        LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"%s[filename:%s,md5:%s] is exists\n",user,filename,md5);
        ret = -2;
        goto END;
    }
    //执行到这里说明该用户不存在此文件
    sprintf(sql_cmd,"select count from file_info where md5 = '%s'",md5);
    ret2 = process_result_one(conn,sql_cmd,tmp);
    if(ret2 != 0){
        LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"%s err\n",sql_cmd);
        ret = -1;
        goto END;
    }
    int count = atoi(tmp);

    //1.将该文件的引用技术 + 1
    sprintf(sql_cmd,"udapte file_info set count = '%s' where md5 = '%s'",count + 1,md5);
    if(mysql_query(conn,sql_cmd) != 0){
        LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"%s err\n",sql_cmd);
        ret = -1;
        goto END;
    }

    //2.在该用户的用户表中插入对应的记录
    struct timeval tv;
    struct tm* ptm = NULL;
    char time_str[128];
    gettimeofday(&tv,NULL);
    ptm = localtime(&tv.tv_sec);
    strftime(time_str,sizeof(time_str),"%Y-%m-%d %H:%M:%S",ptm);
    sprintf(sql_cmd, "insert into user_file_list(user, md5, create_time, file_name, shared_status, pv) values ('%s', '%s', '%s', '%s', %d, %d)", user, md5, time_str, filename, 0, 0);
    if(mysql_query(conn,sql_cmd) != 0){
        LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"%s err\n",sql_cmd);
        ret = -1;
        goto END;
    }

    //3.对用户文件数表进行更新
    sprintf(sql_cmd,"select count from user_file_count where user = '%s'",user);
    count = 0;
    ret2 = process_result_one(conn,sql_cmd,tmp);
    if(ret2 == 1){
        sprintf(sql_cmd,"insert into user_file_count(user,count) values('%s',%d)",user,1);
    }else if(ret2 == 0){
        count = atoi(tmp);
        sprintf(sql_cmd,"udpate user_file_count set count = %d where user = '%s'",count + 1,user);
    }
    if(mysql_query(conn,sql_cmd) != 0){
        LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"%s err\n",sql_cmd);
        ret = -1;
        goto END;
    }

END:
    /*
       返回给客户端的json串
       成功:{"code":"020"} 
       文件已存在:{"code":"021"}
       失败:{"code":"022"}
    */

   out = NULL;
   if(ret == 0){
    out = return_status("020");
   }else if(ret == -1){
    out = return_status("022");
   }else if(ret == -2){
    out = return_status("021");
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
    char cmd[20];
    char user[USER_NAME_LEN];
    char md5[MD5_LEN];
    char filename[FILE_NAME_LEN];

    read_cfg();

    while(FCGI_Accept() >= 0){
        char* query = getenv("QUERY_STRING");
        query_parse_key_value(query,"cmd",cmd,NULL);
        LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"cmd = %s\n",cmd);

        char* contentLength = getenv("CONTENT_LENGTH");
        int len = 0;
        printf("Content-type: text/html\r\n\r\n");

        if(contentLength == NULL){
            len = 0;
        }else{
            len = atoi(contentLength);
        }
        if(len <= 0){
            printf("No data stdandrad input \n");
            LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"len = 0,No data from stdandard input\n");
        }else{
            char buf[4 * 1024] = {0};
            int ret = 0;
            ret= fread(buf,1,len,stdin);
            if(ret == 0){
                LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"fread(buf,1,len,stdin) err\n");
                continue;
            }
            LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"buf = %s\n",buf);

            get_json_info(buf,user,md5,filename);
            LOG(DEALSHAREFILE_LOG_MODULE,DEALSHAREFILE_LOG_PROC,"user = '%s',md5 = '%s',filename = '%s'\n",user,md5,filename);

            if(strcmp(cmd,"pv") == 0){
                pv_file(md5,filename);
            }else if(strcmp(cmd,"cancel") == 0){
                cancel_share_file(user,md5,filename);
            }else if(strcmp(cmd,"save") == 0){
                save_file(user,md5,filename);
            }
        }
    }
    return 0;
}