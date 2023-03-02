set(REG_SRC
    reg_cgi.c
    ../common/make_log.c
    ../common/util_cgi.c
    ../common/cJSON.c
    ../common/deal_mysql.c
    ../common/redis_op.c
    ../common/cfg.c
   )

set(LOGIN_SRC
    login_cgi.c
    ../common/make_log.c
    ../common/cJSON.c
    ../common/deal_mysql.c
    ../common/redis_op.c
    ../common/cfg.c
    ../common/util_cgi.c
    ../common/des.c
    ../common/base64.c
    ../common/md5.c
   )

set(MD5_SRC
    md5_cgi.c
    ../common/make_log.c
    ../common/util_cgi.c
    ../common/cJSON.c
    ../common/deal_mysql.c
    ../common/redis_op.c
    ../common/cfg.c
   )

set(UPLOAD_SRC
    upload_cgi.c
    ../common/make_log.c
    ../common/util_cgi.c
    ../common/cJSON.c
    ../common/deal_mysql.c
    ../common/redis_op.c
    ../common/cfg.c
   )

set(MYFILES_SRC
    myfiles_cgi.c
    ../common/make_log.c
    ../common/util_cgi.c
    ../common/cJSON.c
    ../common/deal_mysql.c
    ../common/redis_op.c
    ../common/cfg.c
   )
set(DEALFILE_SRC
    dealfile_cgi.c
    ../common/make_log.c
    ../common/util_cgi.c
    ../common/cJSON.c
    ../common/deal_mysql.c
    ../common/redis_op.c
    ../common/cfg.c
   )

set(DEALSHAREFILE_SRC
    dealsharefile_cgi.c
    ../common/make_log.c
    ../common/util_cgi.c
    ../common/cJSON.c
    ../common/deal_mysql.c
    ../common/redis_op.c
    ../common/cfg.c
   )

set(SHAREFILES_SRC
    sharefiles_cgi.c
    ../common/make_log.c
    ../common/util_cgi.c
    ../common/cJSON.c
    ../common/deal_mysql.c
    ../common/redis_op.c
    ../common/cfg.c
   )

set(LIBS 
    fdfsclient
    fastcommon
    hiredis_vip
    fcgi
    mysqlclient
    m
   )

add_executable(register  ${REG_SRC})
target_link_libraries(register ${LIBS})

add_executable(login ${LOGIN_SRC})
target_link_libraries(login ${LIBS})

add_executable(md5 ${MD5_SRC})
target_link_libraries(md5 ${LIBS})

add_executable(upload ${UPLOAD_SRC})
target_link_libraries(upload ${LIBS})

add_executable(myfiles ${MYFILES_SRC})
target_link_libraries(myfiles ${LIBS})

add_executable(dealfile ${DEALFILE_SRC})
target_link_libraries(dealfile ${LIBS})

add_executable(dealsharefile ${DEALSHAREFILE_SRC})
target_link_libraries(dealsharefile ${LIBS})

add_executable(sharefiles ${SHAREFILES_SRC})
target_link_libraries(sharefiles ${LIBS})

