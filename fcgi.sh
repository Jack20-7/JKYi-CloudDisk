#!/bin/bash

START=1
STOP=1

case $1 in
    start)
      START=1
      STOP=0
      ;;
    stop)
      START=0
      STOP=1
      ;;
    "")
      STOP=1
      START=1
      ;;
    *)
      STOP=0
      START=0
      ;;
esac

## 首先需要先杀死正在运行的cgi程序
if [ "$STOP" -eq 1 ];then
   #登录
   kill -9 $(ps -aux | grep "./bin_cgi/login" | grep -v grep | awk '{print $2}') > /dev/null 2>&1
   #注册
   kill -9 $(ps -aux | grep "./bin_cgi/register" | grep -v grep | awk '{print $2}') > /dev/null 2>&1
   #上传文件
   kill -9 $(ps -aux | grep "./bin_cgi/upload" | grep -v grep | awk '{print $2}') > /dev/null 2>&1
   # MD5 秒传
   kill -9 $(ps aux | grep "./bin_cgi/md5" | grep -v grep | awk '{print $2}') > /dev/null 2>&1
   # 我的文件
   kill -9 $(ps aux | grep "./bin_cgi/myfiles" | grep -v grep | awk '{print $2}') > /dev/null 2>&1
   # 分享删除文件
   kill -9 $(ps aux | grep "./bin_cgi/dealfile" | grep -v grep | awk '{print $2}') > /dev/null 2>&1
   # 共享文件列表
   kill -9 $(ps aux | grep "./bin_cgi/sharefiles" | grep -v grep | awk '{print $2}') > /dev/null 2>&1
   # 共享文件pv字段处理、取消分享、转存文件
   kill -9 $(ps aux | grep "./bin_cgi/dealsharefile" | grep -v grep | awk '{print $2}') > /dev/null 2>&1

   echo "CGI Programs has killed completely ... "
fi

##重新启动CGI程序

if [ "$START" -eq 1 ];then
   #注册
   echo -n "Register:"
   spawn-fcgi -a 127.0.0.1 -p 9000 -f ./bin_cgi/register
   #登录
   echo -n "Login:"
   spawn-fcgi -a 127.0.0.1 -p 9001 -f ./bin_cgi/login
   #上传
   echo -n "upload:"
   spawn-fcgi -a 127.0.0.1 -p 9002 -f ./bin_cgi/upload
   #秒传
   echo -n "md5:"
   spawn-fcgi -a 127.0.0.1 -p 9003 -f ./bin_cgi/md5
   #展示用户的文件
   echo -n "myfiles:"
   spawn-fcgi -a 127.0.0.1 -p 9004 -f ./bin_cgi/myfiles
   #对普通文件进行分享、删除和下载操作
   echo -n "dealfile:"
   spawn-fcgi -a 127.0.0.1 -p 9005 -f ./bin_cgi/dealfile
   #对共享文件进行取消分享、下载和转存
   echo -n "dealsharefile:"
   spawn-fcgi -a 127.0.0.1 -p 9006 -f ./bin_cgi/dealsharefile
   #展示共享文件
   echo -n "sharefiles:"
   spawn-fcgi -a 127.0.0.1 -p 9007 -f ./bin_cgi/sharefiles

fi

