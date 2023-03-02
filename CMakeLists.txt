cmake_minimum_required(VERSION 3.10)
project(JKYi-CloudDisk)

include_directories(/home/admin/JKYi-CloudDisk/include)
include_directories(/usr/local/include/hiredis-vip)
include_directories(/usr/local/mysql)
include_directories(/home/admin/fastdfs-6.06/client)
include_directories(/home/admin/fastdfs-6.06/storage)
include_directories(/home/admin/fastdfs-6.06/tracker)
include_directories(/home/admin/fastdfs-6.06/common)
include_directories(/usr/include/fastdfs)
include_directories(/usr/include/fastcommon)

link_directories(/usr/local/lib)
link_directories(/usr/local/mysql/lib)
link_directories(/home/admin/fastdfs-6.06/client/)
link_directories(/home/admin/libfastcommon-1.0.43/src)


SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin_cgi)

add_subdirectory(src_cgi)
#add_subdirectory(test)
