#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "make_log.h"
#include "util_cgi.h"
#include "redis_op.h"
#include "cfg.h"

#include "cJSON.h"

int trim_space(char* inbuf){
    int i = 0;
    int j = strlen(inbuf) - 1;
    char* str = inbuf;

    if(inbuf == NULL){
        return -1;
    }

    while(isspace(str[i]) && str[i] != '\0'){
        i++;
    }

    while(isspace(str[j]) && j > i){
        j--;
    }
    
    int count = j - i + 1;

    strncpy(inbuf,str + i,count);
    inbuf[count] = '\0';

    return 0;
}

char* memstr(char* full_name,int full_name_len,char* substr){
    if(full_name == NULL || full_name_len <= 0 || substr == NULL){
        return NULL;
    }

    if(*substr == '\0'){
        return NULL;
    }

    int substrLen = strlen(substr);
    int i = 0;
    char* cur = full_name;
    int last_possible = full_name_len - substrLen + 1;

    for(i = 0;i < last_possible;++i){
        if(*cur == *substr){
            if(memcmp(cur,substr,substrLen) == 0){
                return cur;
            }
        }
        cur++;
    }

    return NULL;
}

int query_parse_key_value(const char* query,const char* key,char* value,int* value_len_p){
    char* tmp = NULL;
    char* end = NULL;
    int value_len = 0;

    tmp = strstr(query,key);
    if(tmp == NULL){
        return -1;
    }

    tmp += strlen(key);
    tmp ++;

    end = tmp;
    while('\0' != *end && *end != '&' && *end != '&'){
        end++;
    }

    value_len = end - tmp;
    strncpy(value,tmp,value_len);
    value[value_len] = '\0';

    if(value_len_p != NULL){
        *value_len_p = value_len;
    }

    return 0;
}

int get_file_suffix(const char* file_name,char* suffix){
    const char* p = file_name;
    int len = 0;
    const char* q = NULL;
    const char* k = NULL;

    if(p == NULL){
        return -1;
    }
    if(suffix == NULL){
        return -1;
    }

    q = p;
    while(*q != '\0'){
        q++;
    }

    k = q;
    while(*k != '.' && k != p){
        k--;
    }

    if(*k == '.'){
        k++;
        len = q - k;
        if(len != 0){
            strncpy(suffix,k,len);
            suffix[len] = '\0';
        }else{
            strncpy(suffix,"null",5);
        }
    }else{
        strncpy(suffix,"null",5);
    }
}

void string_replace(char* strSrc,char* strFind,char* strReplace){
    int len = strlen(strFind);
    while(strSrc != '\0'){
        if(*strSrc == *strFind){
            if(strncmp(strSrc,strFind,len)){
                int i = 0;
                char* p = NULL;
                char* q = NULL;
                char* rep = NULL;
                int lastLen = 0;

                //p 和 q都指向替换之后剩余的那一部分字符串的起始位置
                q = strSrc + len; 
                p = q;
                rep = strReplace;

                //计算出剩余部分的长度
                while(*q++ != '\0'){
                    lastLen++;
                }
                //额外分配空间先将剩余部分那部分字符串保存起来，防止被覆盖
                char* tmp = (char*)malloc(lastLen + 1);
                int k = 0;
                for(;k < lastLen;++k){
                    *(tmp + k) = *(p + k);
                }
                *(tmp + lastLen) = '\0';

                //替换操作
                while(*rep != '\0'){
                    *strSrc++ = *rep++;
                }
                //将在剩余的那一部分拷贝回来
                p = strSrc;
                char* ptmp = tmp;
                while(*ptmp != '\0'){
                    *p++ = *ptmp;
                }
                free(tmp);
                *p = '\0';
            }else{
                strSrc++;
            }
        }else{
            strSrc++;
        }
    }
}

char* return_status(char* status_num){
    char* out = NULL;
    cJSON* root = cJSON_CreateObject();
    //生成类似于 {"code","000"}  这种类型的json串
    cJSON_AddStringToObject(root,"code",status_num); 
    out = cJSON_Print(root);

    cJSON_Delete(root);
    return out;
}

int verify_token(char* user,char* token){
    int ret = 0;
    redisContext* redis_conn = NULL;
    char tmp_token[TOKEN_LEN] = {0};

    char redis_ip[HOST_NAME_LEN] = {0};
    char redis_port[10] = {0};

    get_cfg_value(CFG_PATH,"redis","ip",redis_ip);
    get_cfg_value(CFG_PATH,"redis","port",redis_port);

    redis_conn = rop_connectdb_nopwd(redis_ip,redis_port);
    if(redis_conn == NULL){
        LOG(UTIL_LOG_MODULE,UTIL_LOG_PROC,"redis server connect error\n");
        ret = -1;
        goto END;
    }

    //如果能够找到并且相同，那么代表redis中存储的token未过期
    ret = rop_get_string(redis_conn,user,tmp_token);
    if(ret == 0){
        if(strcmp(token,tmp_token) != 0){
            //不相同
            ret = -1;
        }
    }

END:
   if(redis_conn != NULL){
    rop_disconnect(redis_conn);
   }

   return ret;
}
