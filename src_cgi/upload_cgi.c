#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#include "make_log.h"
#include "deal_mysql.h"
#include "fdfs_api.h"
#include "cfg.h"
#include "util_cgi.h"

#include "fcgi_stdio.h"
#include "fcgi_config.h"
#include "fdfs_client.h"
#include "connection_pool.h"

#define UPLOAD_LOG_MODULE "cgi"
#define UPLOAD_LOG_PROC   "upload"

static char mysql_user[128] = {0};
static char mysql_pwd[128] = {0};
static char mysql_db[128] = {0};

//读取mysql的配置文件
void read_cfg(){
    get_cfg_value(CFG_PATH,"mysql","user",mysql_user);
    get_cfg_value(CFG_PATH,"mysql","password",mysql_pwd);
    get_cfg_value(CFG_PATH,"mysql","database",mysql_db);
    LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"mysql user = %s,password = %s,database = %s\n",mysql_user,mysql_pwd,mysql_db);
}

/// @brief 解析发送的http报文携带的数据,并且会暂时先保存到本地，同时获取文件的名字、上传者、文件名、以及md5值
/// @param len  收到的http报文的长度
/// @param user 
/// @param filename 
/// @param md5 
/// @param p_size 
/// @return 
int recv_save_file(long len,char* user,char* filename,char* md5,long* p_size){
    int ret = 0;

    char* file_buf = NULL;
    char* begin = NULL;
    char* p = NULL;
    char* q = NULL;
    char* k = NULL;


    //报文文件头部信息
    char content_text[TEMP_BUF_MAX_LEN] = {0};
    //分界线信息
    char boundary[TEMP_BUF_MAX_LEN] = {0};

    //1.现在本地开辟一块保存数据的空间
    file_buf = (char*)malloc(len);
    if(NULL == file_buf){
        LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"malloc err\n");
        return -1;
    }

    //2.读取数据
    int ret2 = fread(file_buf,1,len,stdin);
    if(ret2 == 0){
        //读取失败
        LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"fread(file_buf,1,len,stdin) err\n");
        ret = -1;
        goto END;
    }

    //3.处理收到的数据格式
    /*
       ------WebKitFormBoundary88asdgewtgewx\r\n
       Content-Disposition: form-data; user="milo"; filename="xxx.jpg"; md5="xxxx"; size=10240\r\n
       Content-Type: application/octet-stream\r\n
       \r\n
       真正的文件内容\r\n
       ------WebKitFormBoundary88asdgewtgewx
    */    
    begin = file_buf;
    p = begin;
    // 跳过分界线
    p = strstr(begin,"\r\n");
    if(NULL == p){
        LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"wrong no boundary\n");
        ret = -1;
        goto END;
    }
    strncpy(boundary,begin,p - begin); //暂存分界线
    boundary[p - begin] = '\0';
    LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"boundary:[%s]\n",boundary);

    p += 2;
    len -= (p - begin);
    begin = p;

    //跳过content-Disposition,
    p = strstr(begin,"\r\n");
    if(NULL == p){
        LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"ERROR:get content test err,no filename\n");
        ret = -1;
        goto END;
    }
    strncpy(content_text,begin,p - begin);
    content_text[p - begin] = '\0';
    LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"content_text:[ %s ]\n",content_text);

    p += 2;
    len -= (p - begin);

    q = begin;
    q = strstr(begin,"user=");
    q += strlen("user=");
    q++;  //跳过"

    k = strchr(q,'"');
    strncpy(user,q,k - q);
    user[k - q] = '\0';    //保存
    trim_space(user);      //去掉两边的空格

    //获取文件名
    begin = k;
    q = begin;
    q = strstr(begin,"filename=");
    q += strlen("filename=");
    q++;
    k = strchr(q,'"');
    strncpy(filename,q,k - q);
    filename[k - q] = '\0';
    trim_space(filename);

    //获取md5值
    begin = k;
    q = begin;
    q = strstr(begin,"md5=");
    q += strlen("md5=");
    q++;
    k = strchr(q,'"');
    strncpy(md5,q,k - q);
    md5[k - q] = '\0';
    trim_space(md5);

    //获取文件大小
    begin = k;
    q = begin;
    q = strstr(begin,"size=");
    q += strlen("size=");  //size的值是数值类型，没有",所以不需要++
    k = strstr(q,"\r\n");
    char tmp[256] = {0};
    strncpy(tmp,q,k - q);
    tmp[k - q] = '\0';

    *p_size = strtol(tmp,NULL,10);//str to long

    begin = p;
    p = strstr(begin,"\r\n");
    p += 4;  //最后一个header的\r\n + 标准报文的\r\n
    len -= (p - begin);

    //下面对报文的内容进行解析
    begin = p;
    p = memstr(begin,len,boundary); //寻找文件内容的结尾
    if(NULL == p){
        LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"memstr(begin,len,boundary) err\n");
        LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"begin:%s\n",begin);
        LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"boundary:%s\n",boundary);
        ret = -1;
        goto END;
    }else{
        p = p - 2; //指向内容的最后一个字符
    }

    //下面是在本地根据文件名创建一个文件，然后将body中携带的数据写入该文件,为之后上传storage做准备
    int fd = 0;
    LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"start open %s\n",filename);
    fd = open(filename,O_CREAT | O_WRONLY,0644);
    if(fd < 0){
        LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"open %s err\n",filename);
        ret = -1;
        goto END;
    }

    LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"write %s,len = %d\n",filename,(p - begin));

    ftruncate(fd,(p - begin));
    write(fd,begin,(p - begin));
    close(fd);
END:
    free(file_buf);
    return ret;
}

//上传文件
int upload_to_dstorage_1(char* filename,char* confpath,char* fieldid){
    char group_name[20];
    ConnectionInfo* pTrackerServer;
    int result = 0;
    int store_path_index;
    ConnectionInfo storageServer;

    log_init();
    g_log_context.log_level = LOG_ERR;
    ignore_signal_pipe(); //忽略pipe信号

    //加载配置文件
    const char* conf_file = confpath;
    if((result = fdfs_client_init(conf_file)) != 0){
        return result;
    }

    //通过配置文件，连接上tracker server
    pTrackerServer = tracker_get_connection();
    if(pTrackerServer == NULL){
        fdfs_client_destroy();
        return errno != 0 ? errno : ECONNREFUSED;
    }
    *group_name = '\0';

    //连接storage server
    if((result =  tracker_query_storage_store(pTrackerServer,\
                        &storageServer,group_name,&store_path_index)) != 0){
         fdfs_client_destroy();
         LOG("FastDFS","upload_file","tracker_query_storage faild,errno = %d,errstr = %s\n",
                                                                            result,STRERROR(result));
        return result; 
    }

    //向storage 上传文件
    result = storage_upload_by_filename1(pTrackerServer,\
                   &storageServer,store_path_index,\
                   filename,NULL,\
                   NULL,0,group_name,fieldid);
    if(0 == result){
        LOG("fastDFS","upload_file","fieldID = %s",fieldid);
    }else{
        LOG("fastDFS","uploa_file","upload_file fail,err no = %d,errstr = %s",result,STRERROR(result));
    }

    //断开连接
    tracker_close_connection_ex(pTrackerServer,true);
    fdfs_client_destroy();

    return result;
}

int upload_to_dstorage(char* filename,char* fieldid){
    int ret = 0;
    pid_t pid;
    int fd[2];

    //常见管道
    if(pipe(fd) < 0){
        LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"pip err\n");
        ret = -1;
        goto END;
    }

    //创建进程

    pid = fork();
    if(pid < 0){
        LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"fork err\n");
        ret = -1;
        goto END;
    }

    if(pid == 0){
        //子进程
        close(fd[0]);
        dup2(fd[1],STDOUT_FILENO); //将标准输出重定向到文件上面去

        //读取client的配置文件
        char fdfs_cli_conf_path[256] = {0};
        get_cfg_value(CFG_PATH,"dfs_path","client",fdfs_cli_conf_path);
        //执行可执行文件fdfs_upload_file.相当于 fdfs_upload_file client.conf fiename
        execlp("fdfs_upload_file","fdfs_upload_file",fdfs_cli_conf_path,filename,NULL);

        //如果执行成功，正常是不会在回来的
        LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"execlp fdfs_upload_file err\n");
        close(fd[1]);
    }else{
        close(fd[1]);
        read(fd[0],fieldid,TEMP_BUF_MAX_LEN);

        trim_space(fieldid);
        if(strlen(fieldid) == 0){
            LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"[upload failed!]\n");
            ret = -1;
            goto END;
        }
        LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"get [%s] success\n",fieldid);
        wait(NULL);  //回收子进程
        close(fd[0]);
    }
END:
    return ret;
}

/// @brief  封装文件存储在分布式系统中的完整url
/// @param fieldid (in)
/// @param fdfs_file_url (out)
/// @return 
int make_file_url(char* fieldid,char* fdfs_file_url){
    int ret = 0;
    char* p = NULL;
    char* q = NULL;
    char* k = NULL;

    char fdfs_file_stat_buf[TEMP_BUF_MAX_LEN] = {0};
    char fdfs_file_host_name[HOST_NAME_LEN] = {0};  //存放storage所在服务器的ip
    
    pid_t pid;
    int fd[2];

    if(pipe(fd) < 0){
        LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"pipe err\n");
        ret = -1;
        goto END;
    }

    pid = fork();
    if(pid < 0){
        LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"fork err\n");
        ret = -1;
        goto END;
    }

    if(pid == 0){
        //子进程
        close(fd[0]);
        dup2(fd[1],STDOUT_FILENO);

        char fdfs_cli_conf_path[256] = {0};
        get_cfg_value(CFG_PATH,"dfs_path","client",fdfs_cli_conf_path);

        //相当于在终端上执行命令
        //fdfs_file_info client.conf fieldid
        execlp("fdfs_file_info","fdfs_file_info",fdfs_cli_conf_path,fieldid,NULL);

        LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"execlp  fdfs_file_info err\n");
        close(fd[1]);
    }else{
        //父进程
        close(fd[1]);

        read(fd[0],fdfs_file_stat_buf,TEMP_BUF_MAX_LEN);

        wait(NULL);
        close(fd[0]);

        //下面就需要根据子进程从管道内发送会来的数据拼接出完整的url路径
        //要得到的格式 --> http://host_name/group1/M00/00/00/xxxxxx
        //子进程执行命令返回的信息如下:
        /* 
           GET FROM SERVER: false
           file type: normal
           source storage id: 0
           source ip address: 47.97.102.181
           file create timestamp: 2023-02-22 14:28:52
           file size: 3325
           file crc32: 1844770147 (0x6df4f563)
        */

        p = strstr(fdfs_file_stat_buf,"source ip address: ");
        q = p + strlen("source ip address: ");
        k = strstr(q,"\n");
        strncpy(fdfs_file_host_name,q,k - q);
        fdfs_file_host_name[k - q] = '\0';

        //读取storage server的端口
        char storage_web_server_port[20] = {0};
        get_cfg_value(CFG_PATH,"storage_web_server","port",storage_web_server_port);
        strcat(fdfs_file_url,"http://");
        strcat(fdfs_file_url,fdfs_file_host_name);
        strcat(fdfs_file_url,":");
        strcat(fdfs_file_url,storage_web_server_port);
        strcat(fdfs_file_url,"/");
        strcat(fdfs_file_url,fieldid);

        LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"file url is :%s\n",fdfs_file_url);
    }
END:
    return ret;
}


//将文件信息存储到mysql中的fileinfo表里面去
int store_fileinfo_to_mysql(char* user,char* filename,char* md5,long size,char* fieldid,
                                                                            char* fdfs_file_url){
    int ret = 0;
    MYSQL* conn = NULL;

    time_t now;
    char create_time[TEMP_BUF_MAX_LEN];
    char suffix[SUFFIX_LEN];
    char sql_cmd[SQL_MAX_LEN];

    conn = mysql_conn(mysql_user,mysql_pwd,mysql_db);
    if(NULL == conn){
        LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"mysql_conn connect err\n");
        ret = -1;
        goto END;
    }

    mysql_query(conn,"set names utf8");
    get_file_suffix(filename,suffix);

    /*
        file_info表的字段有:
        
        ---md5:      文件的md5值
        ---fileid:   文件fileid
        ---url:      文件下载url
        ---size:     文件大小
        ---type:     文件类型，相当于就是文件的后缀
        ---count:    文件的引用
    */

   sprintf(sql_cmd,"insert into file_info (md5,file_id,url,size,type,count)"
                   "values ('%s','%s','%s','%ld','%s','%d')",
                    md5,fieldid,fdfs_file_url,size,suffix,1);

    if(mysql_query(conn,sql_cmd) != 0){
        LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"[%s] insert failed,errstr = %s\n",sql_cmd,mysql_error(conn));
        ret = -1;
        goto END;
    }
    LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"[%s] insert into file_info seccess\n",sql_cmd);

    now = time(NULL);
    strftime(create_time,TIME_STRING_LEN - 1,"%Y-%m-%d %H:%M:%S",localtime(&now));

    //还需要插入到user_file_list里面去
    /*
        user_file_list表字段:
        user:文件所属用户
        md5:文件的md5值 
        create_time:文件创建时间
        file_name:文件名
        shared_status:文件状态，0-没有共享  1:共享
        pv:文件下载的次数
    */
    sprintf(sql_cmd,"insert into user_file_list(user,md5,create_time,file_name,shared_status,pv)"
                    "values('%s','%s','%s','%s',%d,%d)",
                    user,md5,create_time,filename,0,0);
    if(mysql_query(conn,sql_cmd) != 0){
        LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"[%s] insert into user_file_list err,errstr = %s\n",
                                                sql_cmd,mysql_error(conn));
        ret = -1;
        goto END;
    }

    //还有user_file_count表
    sprintf(sql_cmd,"select count from user_file_count where user = '%s'",user);
    int ret2 = 0;
    char tmp[512] = {0};
    int count = 0;
    ret2 = process_result_one(conn,sql_cmd,tmp);
    if(ret2 == 1){
        //没有记录
        sprintf(sql_cmd,"insert into user_file_count(user,count) values('%s',%d)",user,1);
    }else if(ret2 == 0){
        //如果有对应的用户的话，就将文件数 + 1
        count = atoi(tmp);
        sprintf(sql_cmd,"update user_file_count set count = %d where user = '%s'",count + 1,user);
    }

    if(mysql_query(conn,sql_cmd) != 0){
        LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"[%s] failed,errstr = %s\n",sql_cmd,mysql_error(conn));
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
    char filename[FILE_NAME_LEN] = {0};
    char user[USER_NAME_LEN] = {0};
    char md5[MD5_LEN] = {0};
    long size = 0;
    char fieldid[TEMP_BUF_MAX_LEN] = {0};
    char fdfs_file_url[FILE_URL_LEN] = {0};

    read_cfg(); //初始化数据库配置

    while(FCGI_Accept() >= 0){
        char* contentlength = getenv("CONTENT_LENGTH");
        long len = 0;
        int ret = 0;

        printf("Content-type: text/html\r\n\r\n");
        if(contentlength != NULL){
            LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"CONTENT_LENGTH:%s\n",contentlength);
            len = strtol(contentlength,NULL,10);
        }else{
            len = 0;
        }

        if(len <= 0){
            printf("No data from standard input\n");
            LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"len = 0,No data from standard inoput\n");
            ret = -1;
        }else{
            if(recv_save_file(len,user,filename,md5,&size) < 0){
                ret = -1;
                goto END;
            }

            LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"%s start upload[%s,len:%ld]\n",user,filename,len);


            if(upload_to_dstorage(filename,fieldid) < 0){
                ret = -1;
                goto END;
            }

            LOG(UPLOAD_LOG_MODULE,UPLOAD_LOG_PROC,"%s upload successd[%s,len:%ld, md5:%s]\n",user,filename,size,md5);
            //删除本地文件
            unlink(filename);

            //得到完整的url
            if(make_file_url(fieldid,fdfs_file_url) < 0){
                ret = -1;
                goto END;
            }

            //写入数据库
            if(store_fileinfo_to_mysql(user,filename,md5,size,fieldid,fdfs_file_url) < 0){
                ret = -1;
                goto END;
            }
END:
            memset(filename,0,FILE_NAME_LEN);
            memset(user,0,USER_NAME_LEN);
            memset(md5,0,MD5_LEN);
            memset(fieldid,0,TEMP_BUF_MAX_LEN);
            memset(fdfs_file_url,0,FILE_URL_LEN);

            char* out = NULL;
            if(ret == 0){
                //成功
                out = return_status("008");
            }else{
                //失败
                out = return_status("009");
            }

            if(out != NULL){
                printf(out);
                free(out);
            }
        }
    }
    return 0;
}
