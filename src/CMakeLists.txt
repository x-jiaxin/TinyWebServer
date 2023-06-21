include_directories(threadpool)
include_directories(Lock)
include_directories(mysql)

add_executable(main main.cpp)

add_subdirectory(threadpool)

target_link_libraries(main THREAD_POOL)
