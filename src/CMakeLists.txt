#mysql所需的头文件和库所需的目录和库文件可以用/usr/bin/mysql_config来查看

include_directories("/usr/include/mysql")
link_directories("/usr/lib/x86_64-linux-gnu")
set(MYSQL_LIBS
        mysqlclient pthread z m rt atomic ssl crypto dl
        )


include_directories(threadpool mysql Log Lock timer http server config)


#add_library(MyLib "")
#target_sources(MyLib
#        PUBLIC
#        ./mysql/connectpool.cpp
#        ./threadpool/threadpool.cpp
#        ./Log/log.cpp
#        ./http/http_conn.cpp)


aux_source_directory(./threadpool SOURCE)
aux_source_directory(./Log SOURCE)
aux_source_directory(./mysql SOURCE)
aux_source_directory(./http SOURCE)
aux_source_directory(./timer SOURCE)
aux_source_directory(./server SOURCE)
aux_source_directory(./config SOURCE)
add_library(MyLib ${SOURCE})

add_executable(${PROJECT_NAME} TinyWebServer.cpp)

target_link_libraries(${PROJECT_NAME} MyLib ${MYSQL_LIBS})


