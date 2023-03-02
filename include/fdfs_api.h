#ifndef _FDFS_API_H_
#define _FDFS_API_H_

/// @brief 上传文件
/// @param filename //要上传的文件的名字
/// @param field    //storage 返回的field id
/// @return 
int fdfs_upload_file(const char* filename,char* field);

int fdfs_upload_file1(const char* filename,char* field,int size);

#endif