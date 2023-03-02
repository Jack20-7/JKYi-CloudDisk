#ifndef _MAKE_LOG_H_
#define _MAKE_LOG_H_

#include <pthread.h>

/// @brief 向日志文件中写入日志
/// @param path  日志文件
/// @param buf   要写入的内容
/// @return  0
int output_file(char* path,char* buf);

/// @brief //生成目录文件
/// @param path  存放最终生成的目录文件的path
/// @param module_name  //模块名，向fastdfs、fcig之类的
/// @param proc_name    //程序的名称
/// @return  0
int make_path(char* path,char* module_name,char* proc_name);
int dumpmsg_to_file(char* module_name,char* proc_name,const char* filename,
                        int line,const char* funcname,char* fmt, ...);

#ifndef _LOG
#define LOG(module_name,proc_name,x...) \
   do{    \
        dumpmsg_to_file(module_name,proc_name,__FILE__,__LINE__,__FUNCTION__, ##x); \
   }while(0) 
#else
#define LOG(module_name,proc_name,x...)
#endif

//需要通过加锁来解决多线程并发写日志所带来的日志数据错乱的问题
//属于是外部链接，其他源文件中也可以使用
extern pthread_mutex_t ca_log_lock;

#endif