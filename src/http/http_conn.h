//
// Created by jiaxin on 2023/6/21 14:24.
//

#ifndef TINYWEBSERVER_HTTP_CONN_H
#define TINYWEBSERVER_HTTP_CONN_H

#include <cstdio>
#include <map>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <csignal>
#include <cstring>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cassert>
#include <pthread.h>
#include <cstdlib>
#include <cerrno>
#include <cstdarg>
#include <iostream>
#include "lock.h"
#include "connectpool.h"
#include "log.h"
using std::map;
using std::string;
class http_conn
{
public:
    /*文件名最大长度*/
    static const int FILENAME_LEN = 200;
    /*读缓冲区大小*/
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    /*HTTP请求方法*/
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATCH
    };
    /*解析客户端时，主状态机所处的状态 */
    enum CHECK_STATE
    {
        /*分析请求行*/
        CHECK_STATE_REQUESTLINE = 0,
        /*分析头部*/
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT,
    };
    /*服务器处理HTTP请求的可能结果*/
    enum HTTP_CODE
    {
        /*请求不完整，需要继续读取*/
        NO_REQUEST,
        /*获得了一个完整的请求*/
        GET_REQUEST,
        /*客户请求有语法错误*/
        BAD_REQUEST,
        REQUEST,
        NO_RESOURCE,
        /*客户对资源没有足够的访问权限*/
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        /*客户端已关闭连接*/
        CLOSED_CONNECTION
    };
    /*行的读取状态*/
    enum LINE_STATUS
    {
        /*读取到一个完整行*/
        LINE_OK = 0,
        /*数据出错*/
        LINE_BAD,
        /*数据尚不完整*/
        LINE_OPEN
    };

public:
    http_conn();
    ~http_conn();

public:
    /*初始化新接受的连接*/
    void init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
              int close_log, const string &user, const string &passwd,
              const string &sqlname);
    /*关闭连接*/
    void close_conn(bool read_close = true);
    /*处理客户请求*/
    void process();
    /*非阻塞读操作*/
    bool read_once();
    /*非阻塞写操作*/
    bool write();
    sockaddr_in *getaddress()
    {
        return &m_sock_addr;
    }
    void initmysql_result(connectpool *connPool);
    int timer_flag;
    int improv;

private:
    /*初始化连接*/
    void init();
    /*解析HTTP请求*/
    HTTP_CODE process_read();
    /*填充HTTP应答*/
    bool process_write(HTTP_CODE ret);

    /*下面这一组函数被process_read调用以分析HTTP请求*/
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line();
    LINE_STATUS parse_line();

    /*下面这一组函数被process_write调用以填充HTTP应答*/
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    void add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    /*所有socket上的事件都被注册到同一个epoll内核时间表中，所以将epoll文件描述符设置为静态的*/
    static int m_epollfd;
    /*统计用户数量*/
    static int m_user_count;
    MYSQL *m_mysql;
    int m_state;//读为0, 写为1

private:
    /*该HTTP连接的socket和对方的socket地址*/
    int m_sock_fd;
    sockaddr_in m_sock_addr;

    /*读缓冲区*/
    char m_read_buf[READ_BUFFER_SIZE];
    /*标识读缓冲区中已经读入的客户数据的最后一个字节的下一个位置*/
    int m_read_last_next_idx;
    /*当前正在分析的字符位置*/
    int m_checking_idx;
    /*当前正在解析的行的起始位置*/
    int m_start_line_idx;
    /*写缓冲区*/
    char m_write_buf[WRITE_BUFFER_SIZE];
    /*写缓冲区待发送的字节数*/
    int m_write_to_send;

    /*主状态机当前所处的状态*/
    CHECK_STATE m_check_state;
    /*请求方法*/
    METHOD m_method;

    /*客户请求的目标文件的完整路径，其内容等于doc_root + url*/
    char m_real_file_path[FILENAME_LEN];
    /*文件名*/
    char *m_url;
    /*HTTP协议版本号，我们仅支持HTTP/1.1*/
    char *m_version;
    /*主机名*/
    char *m_host;
    /*HTTP请求的消息体的长度*/
    int m_content_length;
    /*HTTP请求是否要求保持连接*/
    bool m_linger;

    /*客户端请求的目标文件被mmap到内存中的起始位置*/
    char *m_file_maddress;
    /*目标文件的状态，文件是否存在、是否为目录、是否可读，并获取文件信息*/
    struct stat m_file_stat {};
    /*采用writev来执行写操作，iv_count表示被写内存块的数量*/
    //    /* Structure for scatter/gather I/O.  */
    //    struct iovec
    //    {
    //        void *iov_base;	/* Pointer to data.  */
    //        size_t iov_len;	/* Length of data.  */
    //    };
    iovec m_iv[2];
    int m_iv_count;

    int cgi;       //是否启用的POST
    char *m_string;//存储请求头数据
    int bytes_to_send;
    int bytes_have_send;
    char *doc_root;

    map<string, string> m_users;
    int m_TRIGMode;
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};
#endif//TINYWEBSERVER_HTTP_CONN_H
