#ifndef _REDIS_KEYS_H_
#define _REDIS_KEYS_H_


#define REDIS_SERVER_IP   "127.0.0.1"
#define REDIS_SERVER_PORT "6379"

/*
共享用户文件有序集合 (ZSET)
key:    FILE_PUBLIC_ZSET
score:  下载次数
value:  md5 + 文件名
*/
#define FILE_PUBLIC_ZSET  "FILE_PUBLIC_ZSET"


/*
field id 和 文件名映射表 (HASH)
key:    FILE_NAME_HASH
field:  field_id
value:  file_name
*/
#define FILE_NAME_HASH    "FILE_NAME_HASH"

#endif