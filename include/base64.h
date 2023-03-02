#ifndef _BASE64_H_
#define _BASE64_H_

//主要用于给password进行编码

/// @brief 对传入的字符串进行base64编码，然后通过第三个参数返回
/// @param bindata    源字符串
/// @param binlength  源字符串长度
/// @param base64     目的字符串
/// @return base64字符串
char* base64_encode(const unsigned char* bindata,int binlength,char* base64);

/// @brief 解码base64
/// @param base64   要解码的base64字符串
/// @param bindata  解码后得到的字符串
/// @return 解码后字符串的长度
int base64_decode(const char* base64,unsigned char* bindata);

#endif