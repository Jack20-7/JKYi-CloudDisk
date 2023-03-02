#!/bin/bash

if [ $# -eq 0 ];then
   echo "please input arguument:"
   echo "start   ---->   start the nginx"
   echo "stop    ---->   stop the nginx"
   echo "reload  ---->   reload the nginx"
   exit 1
fi

case $1 in
    start)
        sudo /usr/local/nginx/sbin/nginx -c /root/nginx-1.23.2/conf/nginx.conf
        if [ $? -eq 0 ];then
           echo "nginx start success ..."
        else
           echo "nginx start failed ..."
        fi
        ;;
    stop)
       sudo /usr/local/nginx/sbin/nginx -s quit
       if [ $? -eq 0 ];then
          echo "nginx stop success..."
        else
          echo "nginx stop failed..."
        fi
        ;;
    reload)
        sudo /usr/local/nginx/sbin/nginx -s reload -c /root/nginx-1.23.2/conf/nginx.conf
        if [ $? -eq 0 ];then
           echo "nginx reload success ..."
        else
           echo "nginx reload failed ..."
        fi
        ;;
    *)
        echo "do nothing ..."
        ;;
esac
