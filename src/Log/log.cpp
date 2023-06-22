//
// Created by jiaxin on 2023/6/21 14:11.
//

#include "log.h"

Log *Log::getInstance()
{
    static Log log;
    return &log;
}

void *Log::flush_log_thread(void *args)
{
    return nullptr;
}

bool Log::init(const char *file_name, int close_log, int log_buf_size,
               int split_lines, int max_queue_size)
{
    return false;
}

bool Log::write_log(int level, const char *format, ...)
{
    return false;
}

void Log::flush() {}

Log::Log() {}

Log::~Log() {}

void *Log::async_write_log()
{
    return nullptr;
}
