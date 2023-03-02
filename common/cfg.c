#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "make_log.h"
#include "cfg.h"

#include "cJSON.h"


int get_cfg_value(const char* profile,char* title,char* key,char* value){
    int ret = 0;
    char* buf = NULL;
    FILE* fp = NULL;

    if(profile == NULL || title == NULL || key == NULL || value == NULL){
        return -1;
    }
    fp = fopen(profile,"rb");
    if(fp == NULL){
        perror("fopen");
        LOG(CFG_LOG_MODULE,CFG_LOG_PROC,"fopen err\n");
        ret = -1;
        goto END;
    }

    fseek(fp,0,SEEK_END);   //偏移量设置到最后面
    long size = ftell(fp);  //获取配置文件的大小
    fseek(fp,0,SEEK_SET);   //重新将偏移量设置到开头

    buf = (char*)calloc(1,size + 1);
    if(buf == NULL){
        perror("calloc");
        LOG(CFG_LOG_MODULE,CFG_LOG_PROC,"calloc error\n");
        ret = -1;
        goto END;
    }

    fread(buf,1 , size,fp);//加载文件内容

    cJSON* root = cJSON_Parse(buf);
    if(NULL == root){
        LOG(CFG_LOG_MODULE,CFG_LOG_PROC,"cJSON root err\n");
        ret = -1;
        goto END;
    }

    cJSON* father = cJSON_GetObjectItem(root,title);
    if(NULL == father){
        LOG(CFG_LOG_MODULE,CFG_LOG_PROC,"title is not exists\n");
        ret = -1;
        goto END;
    }

    cJSON* son = cJSON_GetObjectItem(father,key);
    if(NULL == son){
        LOG(CFG_LOG_MODULE,CFG_LOG_PROC,"key is not exists\n");
        ret = -1;
        goto END;
    }

    strcpy(value,son->valuestring);
    cJSON_Delete(root);

END:
   if(fp != NULL){
    fclose(fp);
   }
   if(buf != NULL){
    free(buf);
   }

   return ret;
}


