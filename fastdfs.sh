#!/bin/bash

#启动tracker的函数
tracker_start()
{
    #首先检查tracker server是否已经在运行
    ps -aux | grep fdfs_trackerd | grep -v grep > /dev/null
    if [ $? -eq 0 ];then
       echo "fdfs_tracker is already running."
    else
       sudo fdfs_trackerd /home/admin/JKYi-CloudDisk/conf/tracker.conf
       if [ $? -ne 0 ];then
          echo "tracker start failed..."
       else
          echo "tracker start success..."
       fi
    fi
}

storage_start()
{
    ps -aux | grep fdfs_storaged | grep -v grep > /dev/null
    if [ $? -eq 0 ];then
       echo "fdfs_storage is already running."
    else
       sudo fdfs_storaged /home/admin/JKYi-CloudDisk/conf/storage.conf
       if [ $? -ne 0 ];then
          echo "storage start failed..."
       else
          echo "storage start success..."
       fi
    fi
}

#二如果传入的参数为空的话
if [ $# -eq 0 ];then
   echo "Operation:"
   echo " start storage please input argument:storage"
   echo " start tracker please input argument:tracker"
   echo " start storage && tracker please input argument:all"
   echo " stop storage && tracker please input argument:stop"
fi

case $1 in
     storage)
        storage_start
        ;;
     tracker)
        tracker_start
        ;;
     all)
        storage_start
        tracker_start
        ;;
     stop)
        sudo fdfs_trackerd ./conf/tracker.conf stop
        sudo fdfs_storaged ./conf/storage.conf stop
        ;;
     *)
        echo "nothing........"
esac