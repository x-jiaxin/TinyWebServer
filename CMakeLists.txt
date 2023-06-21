cmake_minimum_required(VERSION 3.24)
project(TinyWebServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)

#静态库 .a
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/lib)
#可执行文件
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/bin)
#动态库 .so
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/bin)

add_subdirectory(src)

