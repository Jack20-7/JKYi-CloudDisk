#!/bin/bash
echo 
echo ============= FastDFS ============
#关闭掉已经启动的tracker和storage
sh fastdfs.sh stop
#重新启动
sh fastdfs.sh all

echo 
echo =========== fastCGI ============
sh fcgi.sh

echo
echo ============ nginx ==============
sh nginx.sh stop
sh nginx.sh start

echo 
echo ============ redis ==============
sh redis.sh stop
sh redis.sh start
