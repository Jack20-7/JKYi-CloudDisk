#ifndef _UTIL_CGI_H_
#define _UTIL_CGI_H_

/*
 CGI 程序后台通用接口
*/

#define FILE_NAME_LEN      (256)   //文件名长度
#define TEMP_BUF_MAX_LEN   (512)   //临时缓冲区的大小
#define FILE_URL_LEN       (512)   //文件存放的storage server的host_name长度
#define HOST_NAME_LEN      (30)    //主机IP长度
#define USER_NAME_LEN      (128)   //用户名长度
#define TOKEN_LEN          (128)   //登录时需要用到的token的长度
#define MD5_LEN            (256)   //文件md5的长度
#define PWD_LEN            (256)   //密码长度
#define TIME_STRING_LEN    (25)    //时间戳长度
#define SUFFIX_LEN         (8)     //后缀名长度
#define PIC_NAME_LEN       (10)    //图片资源名长度
#define PIC_URL_LEN        (256)   //图片资源url长度

#define UTIL_LOG_MODULE     "cgi"
#define UTIL_LOG_PROC       "util"

/// @brief 去掉一个字符串两边的空白字符
/// @param inbuf 
/// @return  0 成功  -1 失败
int trim_space(char* inbuf);

/// @brief 在full_data中查找substr第一次出现的位置
/// @param full_data 
/// @param full_data_len 
/// @param substr 
/// @return  成功:匹配字符出首地址   失败:NULL
char* memstr(char*  full_data,int full_data_len,char* substr);

/// @brief 解析url中所携带的参数 类似于 abc=123&&def=456 
/// @param query 
/// @param key 
/// @param value 
/// @param value_len 
/// @return 成功:0  失败:-1
int query_parse_key_value(const char* query,const char* key,char* value,int* value_len);

/// @brief 根据文件名，得到文件后缀保存在suffix中
/// @param file_name 
/// @param suffix 
/// @return 
int get_file_suffix(const char* file_name,char* suffix);

/// @brief 将字符串strSrc中的strFind全部替换为strReplace.需要注意的就是该函数默认strSrc所在那一块空间能够容纳下替换后的字符串
/// @param strSrc 
/// @param strFind 
/// @param strReplace 
void string_replace(char* strSrc,char* strFind,char* strReplace);

/// @brief 向前端返回具体的情况
/// @param status_num 
/// @return 
char* return_status(char* status_num);

/// @brief  验证登录的token
/// @param user 
/// @param token 
/// @return  成功:0  失败:-1
int verify_token(char* user,char* token);

#endif