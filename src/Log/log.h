//
// Created by jiaxin on 2023/6/21 14:11.
//

#ifndef TINYWEBSERVER_LOG_H
#define TINYWEBSERVER_LOG_H

#include <iostream>
#include <ctime>
#include <cstring>
#include <cstdarg>
#include <sys/time.h>
#include "lock.h"
#include "block_queue.h"
using std::cin;
using std::cout;
using std::endl;
using std::string;
class Log
{
public:
    static Log *getInstance();

    static void *flush_log_thread(void *args);

    bool init(const char *file_name, int close_log, int log_buf_size = 8192,
              int split_lines = 5000000, int max_queue_size = 0);

    bool write_log(int level, const char *format, ...);

    void flush();

    int get_close() const
    {
        return m_close_log;
    }

private:
    Log();
    //    ?
    virtual ~Log();
    //    asynchronous异步
    void *async_write_log();

private:
    //路径名
    char dir_path[128];
    //log文件名
    char log_name[128];
    //日志最大行数
    int m_max_lines;
    //日志缓冲大小
    int m_log_buf_size;
    //日志行数记录
    long long m_lines;
    //按天分类
    int m_day;
    //打开log文件的文件指针
    FILE *m_file_ptr;
    char *m_buf;
    //阻塞队列
    block_queue<string> *m_block_queue;
    //是否同步标志位
    bool m_is_async;
    locker m_mutex;
    //关闭日志
    int m_close_log;
};
#define LOG_DEBUG(format, ...)                                                 \
    if (0 == Log::getInstance()->get_close())                                  \
    {                                                                          \
        Log::getInstance()->write_log(0, format, ##__VA_ARGS__);               \
        Log::getInstance()->flush();                                           \
    }

#define LOG_INFO(format, ...)                                                  \
    if (0 == Log::getInstance()->get_close())                                  \
    {                                                                          \
        Log::getInstance()->write_log(1, format, ##__VA_ARGS__);               \
        Log::getInstance()->flush();                                           \
    }
#define LOG_WARN(format, ...)                                                  \
    if (0 == Log::getInstance()->get_close())                                  \
    {                                                                          \
        Log::getInstance()->write_log(2, format, ##__VA_ARGS__);               \
        Log::getInstance()->flush();                                           \
    }
#define LOG_ERROR(format, ...)                                                 \
    if (0 == Log::getInstance()->get_close())                                  \
    {                                                                          \
        Log::getInstance()->write_log(3, format, ##__VA_ARGS__);               \
        Log::getInstance()->flush();                                           \
    }
#endif//TINYWEBSERVER_LOG_H
