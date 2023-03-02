#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include "make_log.h"

pthread_mutex_t ca_log_lock = PTHREAD_MUTEX_INITIALIZER;

//创建目录文件并且写入内容
int dumpmsg_to_file(char* module_name,char* proc_name,const char* filename,
                                int line,const char* funcname,char* fmt,...){
    char msg[4096] = {0};
    char buf[4096] = {0};
    char filepath[1024] = {0};

    time_t t = 0;
    struct tm* now = NULL;
    time(&t);
    now = localtime(&t);

    va_list ap;
    va_start(ap,fmt);
    vsprintf(msg,fmt,ap);
    va_end(ap);

    //拼接处要打印的日志中的时间
    snprintf(buf,4096,"[%04d-%02d-%02d %02d:%02d:%02d]--[%s:%d]--%s",
                                now -> tm_year + 1900, now -> tm_mon + 1,                                         
                                now -> tm_mday, now -> tm_hour, now -> tm_min, now -> tm_sec,                     
								filename, line, msg); 

    //生成目录
    make_path(filepath,module_name,proc_name);
    //写入日志
    pthread_mutex_lock(&ca_log_lock);
      output_file(filepath,buf);
    pthread_mutex_unlock(&ca_log_lock);
}

int output_file(char* path,char* buf){
    int fd;
    fd = open(path,O_RDWR | O_CREAT | O_APPEND,0777);
    if(write(fd,buf,strlen(buf)) != (int)strlen(buf)){
        fprintf(stderr,"write log error\n");
    }
    close(fd);
    return 0;
}

//在对应的目录下生成日志文件
int make_path(char* path,char* module_name,char* proc_name){
    time_t t;
	struct tm *now = NULL;
	char top_dir[1024] = {"."};
	char second_dir[1024] = {"./logs"};
	char third_dir[1024] = {0};
	char y_dir[1024] = {0};
	char m_dir[1024] = {0};
	char d_dir[1024] = {0}; 
	time(&t);
        now = localtime(&t);
	snprintf(path, 1024, "./logs/%s/%04d/%02d/%s-%02d.log", module_name, now -> tm_year + 1900, now -> tm_mon + 1, proc_name, now -> tm_mday);
	
	sprintf(third_dir, "%s/%s", second_dir, module_name);
	sprintf(y_dir, "%s/%04d/", third_dir, now -> tm_year + 1900);
	sprintf(m_dir, "%s/%02d/", y_dir, now -> tm_mon + 1);
	sprintf(d_dir,"%s/%02d/", m_dir, now -> tm_mday);
	
	if(access(top_dir, 0) == -1) {
		if(mkdir(top_dir, 0777) == -1) {
			fprintf(stderr, "create %s failed!\n", top_dir);	
		} else if(mkdir(second_dir, 0777) == -1) {
			fprintf(stderr, "%s:create %s failed!\n", top_dir, second_dir);
		} else if(mkdir(third_dir, 0777) == -1) {
			fprintf(stderr, "%s:create %s failed!\n", top_dir, third_dir);
		} else if(mkdir(y_dir, 0777) == -1) {
                        fprintf(stderr, "%s:create %s failed!\n", top_dir, y_dir);                                                     
                } else if(mkdir(m_dir, 0777) == -1) {                                                             
                        fprintf(stderr, "%s:create %s failed!\n", top_dir, m_dir);                                                     
                }          	
	} else if(access(second_dir, 0) == -1) {
		if(mkdir(second_dir, 0777) == -1) {
			fprintf(stderr, "create %s failed!\n", second_dir);
		} else if(mkdir(third_dir, 0777) == -1) {
			fprintf(stderr, "%s:create %s failed!\n", second_dir, third_dir);
                } else if(mkdir(y_dir, 0777) == -1) {
                        fprintf(stderr, "%s:create %s failed!\n", second_dir, y_dir);
                } else if(mkdir(m_dir, 0777) == -1) {
                        fprintf(stderr, "%s:create %s failed!\n", second_dir, m_dir);
                }
	} else if(access(third_dir, 0) == -1) {
		if(mkdir(third_dir, 0777) == -1) {
			fprintf(stderr, "create %s failed!\n", third_dir);
		} else if(mkdir(y_dir, 0777) == -1) {
			fprintf(stderr, "%s:create %s failed!\n", third_dir, y_dir);
		} else if(mkdir(m_dir, 0777) == -1) {
			fprintf(stderr, "%s:create %s failed!\n", third_dir, m_dir);
		} 
	} else if (access(y_dir, 0) == -1) {
		if(mkdir(y_dir, 0777) == -1) {
			fprintf(stderr, "create %s failed!\n", y_dir);
		} else if(mkdir(m_dir, 0777) == -1) {
                        fprintf(stderr, "%s:create %s failed!\n", y_dir, m_dir);
                } 

	} else if (access(m_dir, 0) == -1) {
                if(mkdir(m_dir, 0777)) {
			fprintf(stderr, "create %s failed!\n", m_dir);
		} 
        }
	return 0;
}