//
// Created by jiaxin on 2023/6/21 14:11.
//

#include "log.h"

Log *Log::getInstance()
{
    //C++11以后,使用局部变量懒汉不用加锁
    static Log log;
    return &log;
}

void *Log::flush_log_thread(void *args)
{
    Log::getInstance()->async_write_log();
}

/*异步需要设置阻塞队列的长度，同步不需要设置*/
bool Log::init(const char *file_name, int close_log, int log_buf_size,
               int split_lines, int max_queue_size)
{
    //异步
    if (max_queue_size >= 1)
    {
        m_is_async = true;
        m_block_queue = new block_queue<string>(max_queue_size);
        pthread_t tid;
        // flush_log_thread为回调函数
        pthread_create(&tid, nullptr, flush_log_thread, nullptr);
    }
    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    m_max_lines = split_lines;

    auto t = time(nullptr);
    auto sys_tm = localtime(&t);
    auto my_tm = *sys_tm;

    //在参数 str 所指向的字符串中搜索最后一次出现字符 c（一个无符号字符）的位置。
    auto p = strrchr(file_name, '/');
    char log_full_name[256]{0};

    if (!p)
    {
        //snprintf() 是一个 C 语言标准库函数，用于格式化输出字符串，并将结果写入到指定的缓冲区，
        // 与 sprintf() 不同的是，snprintf() 会限制输出的字符数，避免缓冲区溢出。
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900,
                 my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else
    {
        strcpy(log_name, p + 1);
        strncpy(dir_path, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_path,
                 my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                 log_name);
    }

    m_day = my_tm.tm_mday;
    //追加到一个文件。写操作向文件末尾追加数据。如果文件不存在，则创建文件。
    m_file_ptr = fopen(log_full_name, "a");
    if (!m_file_ptr)
    {
        return false;
    }
    return true;
}

bool Log::write_log(int level, const char *format, ...)
{
    timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t t = now.tv_sec;
    tm *sys_tm = localtime(&t);
    tm my_tm = *sys_tm;
    char s[16]{0};
    switch (level)
    {
        case 0:
            strcpy(s, "[debug]:");
            break;
        case 1:
            strcpy(s, "[info]:");
            break;
        case 2:
            strcpy(s, "[warn]:");
            break;
        case 3:
            strcpy(s, "[error]:");
            break;
        default:
            strcpy(s, "[info]:");
            break;
    }
    //写入一个log，对m_count++, m_split_lines最大行数
    m_mutex.lock();
    ++m_lines;
    if (m_day != my_tm.tm_mday || m_lines % m_max_lines == 0)
    {
        char new_log[256]{0};
        fflush(m_file_ptr);
        fclose(m_file_ptr);
        char tail[16] = {0};

        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900,
                 my_tm.tm_mon + 1, my_tm.tm_mday);

        if (m_day != my_tm.tm_mday)
        {
            snprintf(new_log, 255, "%s%s%s", dir_path, tail, log_name);
            m_day = my_tm.tm_mday;
            m_lines = 0;
        }
        else
        {
            snprintf(new_log, 255, "%s%s%s.%lld", dir_path, tail, log_name,
                     m_lines / m_max_lines);
        }
        m_file_ptr = fopen(new_log, "a");
        //        printf("new_log: %s\n", new_log);
    }
    m_mutex.unlock();

    va_list valist;
    va_start(valist, format);

    string log_str;
    m_mutex.lock();

    //写入具体的时间
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    int m = vsnprintf(m_buf + n, m_log_buf_size - n - 1, format, valist);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;
    m_mutex.unlock();

    if (m_is_async && !m_block_queue->full())
    {
        printf("async\n");
        m_block_queue->push(log_str);
        printf("log_str: %s\n", log_str.c_str());
    }
    else
    {
        m_mutex.lock();
        fputs(log_str.c_str(), m_file_ptr);
        m_mutex.unlock();
    }
    va_end(valist);
}

void Log::flush()
{
    m_mutex.lock();
    //fflush 则会将流缓冲区的内容写入基础文件或设备，并且该缓冲区将被丢弃
    fflush(m_file_ptr);
    m_mutex.unlock();
}

Log::Log()
{
    m_lines = 0;
    m_is_async = false;
}

Log::~Log()
{
    printf("~log\n");
    if (m_file_ptr)
    {
        fclose(m_file_ptr);
    }
    if (m_block_queue)
        delete m_block_queue;
}

void *Log::async_write_log()
{
    string log_str;
    while (m_block_queue->pop(log_str))
    {
        m_mutex.lock();
        fputs(log_str.c_str(), m_file_ptr);
        m_mutex.unlock();
    }
}
