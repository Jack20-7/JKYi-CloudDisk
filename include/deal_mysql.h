#ifndef _DEAL_MYSQL_H_
#define _DEAL_MYSQL_H_

#include "include/mysql.h"

//最大SQL语句的长度
#define SQL_MAX_LEN  (512)

/// @brief 打印数据库操作出错时的错误信息
/// @param conn  连接的句柄
/// @param title 打印的错误信息
void print_error(MYSQL* conn,const char* title);

/// @brief 与数据库建立连接
/// @param user_name  连接的用户名
/// @param passwd     连接的密码
/// @param db_name    使用的数据库名
/// @return 建立好连接的句柄
MYSQL* mysql_conn(char* user_name,char* passwd,char* db_name);

/// @brief 处理数据库查询请求结果,主要用于debug.
/// @param conn 连接的句柄
/// @param res_set  要处理的结果集
void process_result_test(MYSQL* conn,MYSQL_RES* res_set);

/// @brief 处理数据库查询结果,处理出的结果集保存在buf中,只处理一条记录，一个字段
/// @param conn 
/// @param sql 
/// @param buf 
/// @return 0-成功并且保存了结果集  1-没有结果集  2-有结果集但是未保存到buf中 -1-失败
int process_result_one(MYSQL* conn,char* sql,char* buf);

#endif