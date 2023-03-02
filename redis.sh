#!/bin/bash

NAME=redis
FILE=redis_6379.pid

#判断redis目录是否存在，如果不存在的话就创建
is_directory()
{
    if [ ! -d $1 ];then
       echo "creating $1 direstory..."
       mkdir $1
       if [ $? -eq 0 ];then
          echo "$1 directory is created failed."
          exit 1
       fi
    fi
}

#判断redis文件是否存在，不存在的话就创建
is_regfile()
{
    if [ ! -f $1 ];then
       echo "$1 file not exit ..."
       return 1
    fi
    return 0
}


if [ $# -eq 0 ];then
   echo "please input argument:"
   echo "start ---->  start redis server"
   echo "stop  ---->  stop redis server"
   echo "status ----> show the redis server statsu"
   exit 1
fi

is_directory $NAME

case $1 in 
   start)
       ps -axu | grep redis-server | grep -v grep > /dev/null
       if [ $? -eq 0 ];then
          echo "redis_server is running ..."
       else
          #如果redis_server没有运行的话
          #删除掉redis文件
          unlink "$NAME/$FILE"

          echo "redis_server starting ..."
          /usr/local/bin/redis-server /home/admin/JKYi-CloudDisk/conf/redis_6379.conf
          if [ $? -eq 0 ];then
             echo "redis server start success ..."
             sleep 1
             if is_regfile "$NAME/$FILE";then
                printf "****** Redis server PID: [ %s ] ******\n" $(cat "$NAME/$FILE")
                printf "****** Redis server PORT: [ %s ] ******\n" $(awk '/^port /{print $2}' "./conf/redis_6379.conf")
             fi
          fi
      fi
      ;;
   stop)
        ps -axu | grep redis-server | grep -v grep > /dev/null
        if [ $? -ne 0 ];then
           echo "redis-server is not running ..."
           exit 1
        fi

        echo "redis-server stopping ..."
        if is_regfile "$NAME/$FILE";then
           echo "### stop redis by redis_pid file"
           PID=$(cat "$NAME/$FILE")
        else
           echo "### stop redis by ps"
           PID=$(ps -axu | grep redis-server | grep -v grep | awk '{print $2}')
        fi
        echo "redis-server pid = $PID"
        kill -9 $PID
        if [ $? -eq 0 ];then
           echo "redis-server stop success ..."
        else
           echo "redis-server stop failed ..."
        fi
        ;;
    status)
        ps -aux | grep redis-server | grep -v grep > /dev/null
        if [ $? -eq 0 ];then
           echo "redis server is running ..."
        else
           echo "redis server is not running ..."
        fi
        ;;
    *)
        echo "do nothing ..."
        ;;
esac

           





   
