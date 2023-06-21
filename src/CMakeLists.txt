include_directories(threadpool)
include_directories(Lock)
include_directories(Log)
include_directories(mysql)

add_executable(main main.cpp)


aux_source_directory(./threadpool SOURCE)
aux_source_directory(./Log SOURCE)
aux_source_directory(./mysql SOURCE)
aux_source_directory(./http SOURCE)

add_library(MyLib ${SOURCE})
target_link_libraries(main MyLib)
