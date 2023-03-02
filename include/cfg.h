#ifndef _CFG_H_
#define _CFG_H_

#define CFG_PATH  "./conf/cfg.json"  //需要解析的配置文件路径

#define CFG_LOG_MODULE "cgi"
#define CFG_LOG_PROC   "cfg"

/// @brief 从配置文件中读取我们想要的参数
/// @param profile 配置文件的路径
/// @param tile    配置文件title名称
/// @param key     key
/// @param value   读取到的值
/// @return 
extern int get_cfg_value(const char* profile,char* tile,char* key,char* value);

#endif