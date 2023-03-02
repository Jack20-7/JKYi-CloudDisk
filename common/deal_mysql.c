#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deal_mysql.h"

void print_error(MYSQL* conn,const char* title){
    fprintf(stderr,"%s:\nError %u (%s)\n",title,mysql_errno(conn),mysql_error(conn));
}

MYSQL* mysql_conn(char* user_name,char* passwd,char* db_name){
    MYSQL* conn = NULL;
    conn = mysql_init(NULL);
    if(NULL == conn){
        fprintf(stderr,"mysql init failed\n");
        return NULL;
    }
    if(mysql_real_connect(conn,NULL,user_name,passwd,db_name,0,NULL,0) == NULL){
        fprintf(stderr,"mysql_real_connect failed,Error = %u(%s)\n");
        mysql_close(conn);
        return NULL;
    }
    return conn;
}

void process_result_test(MYSQL* conn,MYSQL_RES* result){
    MYSQL_ROW row;
    uint i = 0;

    //从结果集中拿出一条记录放在MYSQL_ROW中
    while((row = mysql_fetch_row(result)) != NULL){
        //mysql_num_fields用来获取记录中字段的个数
        for(i = 0;i < mysql_num_fields(result);++i){
            if(i > 0){
                fputc('\t',stdout);
            }
            printf("%s",row[i] != NULL ? row[i] : "NULL");
        }
        fputc('\n',stdout);
    }
    if(mysql_errno(conn) != 0){
        print_error(conn,"mysql_fetch_row() failed");
    }else{
        printf("%lu rows returned \n",(ulong)mysql_num_rows(result));
    }
}

int process_result_one(MYSQL* conn,char* sql,char* buf){
    int ret = 0;
    MYSQL_RES* result = NULL;

    if(strlen(sql) > SQL_MAX_LEN){
        fprintf(stderr,"sql length > 512 byte");
        ret = -1;
        goto END;
    }
    //执行传入的sql语句
    if(mysql_query(conn,sql) != 0){
        print_error(conn,"mysql_query failed\n");
        ret -1;
        goto END;
    }

    //语句执行成功的话,获取结果集
    result = mysql_store_result(conn);
    if(result == NULL){
        print_error(conn,"mysql_store_result failed\n");
        ret = -1;
        goto END;
    }

    MYSQL_ROW row;
    ulong line = 0;
    line = mysql_num_rows(result);  //获取结果集中记录的条数
    if(line == 0){
        ret = 1; //没有记录
        goto END;
    }else if(line > 0 && buf == NULL){
        ret = 2;  //有记录，但是不需要保存
        goto END;
    }

    //下面的情况就是有记录并且需要进行保存
    if((row = mysql_fetch_row(result)) != NULL){
        //只能一条记录,并且只有一个字段
        if(row[0] != NULL){
            strcpy(buf,row[0]);
        }
    }

END:
   if(result != NULL){
       mysql_free_result(result);
   }

   return ret;
}