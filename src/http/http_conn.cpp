//
// Created by jiaxin on 2023/6/21 14:24.
//

#include "http_conn.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <mysql/mysql.h>
using std::pair;
using std::string;
/*定义HTTP响应的一些状态信息*/
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form =
        "Your request has bad syntax or is inherently impossible to satisfy.\n";

const char *error_403_title = "Forbidden";
const char *error_403_form =
        "You do not have permission to get file from this server.\n";

const char *error_404_title = "Not Found";
const char *error_404_form =
        "The requested file was not found on this server.\n";

const char *error_500_title = "Internal Error";
const char *error_500_form =
        "There was an unusual problem serving the requested file.\n";

/*网站的根目录*/
/*const char *doc_root = "/home/jiaxin/Code/NiuKe_LinuxWebService/"
                       "LinuxNetProgram/15webserver/resources";*/
locker m_lock;
//存放user表的用户名密码
map<string, string> users;

void setnonblocking(int fd)
{
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

void addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event{};
    event.data.fd = fd;
    if (TRIGMode)
    {
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }
    else
    {
        event.events = EPOLLIN | EPOLLRDHUP;
    }
    /*一个时刻只能一个线程处理socket连接*/
    if (one_shot)
    {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
}

void modfd(int epollfd, int fd, int ev, int TRIGMode)
{
    epoll_event event{};
    event.data.fd = fd;
    if (TRIGMode)
    {
        /*错出在这，导致写事件无效*/
        //        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        event.events = ev | EPOLLET | EPOLLRDHUP;
    }
    else
    {
        event.events = ev | EPOLLRDHUP;
    }
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

http_conn::http_conn() = default;
http_conn::~http_conn() = default;

void http_conn::init(int sockfd, const sockaddr_in &addr, char *root,
                     int TRIGMode, int close_log, const string &user,
                     const string &passwd, const string &sqlname)
{
    m_sock_fd = sockfd;
    m_sock_addr = addr;
    /*如下两行是为了避免TIME_WAIT状态，仅用于调试，实际使用时应该去掉*/
    /* int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));*/
    m_TRIGMode = TRIGMode;

    addfd(m_epollfd, sockfd, true, m_TRIGMode);
    ++m_user_count;

    //当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
    doc_root = root;
    m_close_log = close_log;
    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());
    init();
}
void http_conn::close_conn(bool real_close)
{
    if (real_close && (m_sock_fd != -1))
    {
        removefd(m_epollfd, m_sock_fd);
        m_sock_fd = -1;
        --m_user_count;
    }
}

/*循环读取客户数据，直到无数据可读或对方关闭连接
非阻塞ET工作模式下，需要一次性将数据读完*/
bool http_conn::read_once()
{
    /*循环读，直到连接关闭或无数据可读*/
    if (m_read_last_next_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read;
    /*LT读取数据*/
    if (!m_TRIGMode)
    {
        bytes_read = (int)recv(m_sock_fd, m_read_buf + m_read_last_next_idx,
                               READ_BUFFER_SIZE - m_read_last_next_idx, 0);
        if (bytes_read <= 0)
        {
            return false;
        }
        m_read_last_next_idx += bytes_read;
        return true;
    }
    /*ET*/
    else
    {
        while (true)
        {
            bytes_read = (int)recv(m_sock_fd, m_read_buf + m_read_last_next_idx,
                                   READ_BUFFER_SIZE - m_read_last_next_idx, 0);
            if (bytes_read == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    break;
                }
                return false;
            }
            else if (bytes_read == 0)
            {
                return false;
            }
            m_read_last_next_idx += bytes_read;
        }
        return true;
    }
}

/*写HTTP响应*/
bool http_conn::write()
{
    int temp;
    if (bytes_to_send == 0)
    {
        /*EPOLLIN: 数据可读*/
        modfd(m_epollfd, m_sock_fd, EPOLLIN, m_TRIGMode);
        init();
        return true;
    }
    while (true)
    {
        temp = (int)writev(m_sock_fd, m_iv, m_iv_count);
        // write error
        if (temp <= -1)
        {
            /*如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件。
             * 虽然在此期间，服务器无法立即接收到同一客户的下一个请求，
             * 但这可以保证连接的完整性*/
            // EAGAIN: 事件未发生
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sock_fd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            unmap();
            return false;
        }
        bytes_to_send -= temp;
        bytes_have_send += temp;
        //m_iv[0]指向m_write_buf（存放响应报文头部内容），m_iv[1]指向m_file_address（使用mmap将请求资源映射到的内存）
        //，然后可以使用writev按顺序将其写到socket中。
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base =
                    m_file_maddress + (bytes_have_send - m_write_needto_send);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }
        if (bytes_to_send <= 0)
        {
            /*发送HTTP响应成功，根据HTTP请求中的Connection字段决定是否立即关闭连接*/
            unmap();
            modfd(m_epollfd, m_sock_fd, EPOLLIN, m_TRIGMode);

            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}

void http_conn::init()
{
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;

    m_method = GET;
    m_url = nullptr;
    m_version = nullptr;
    m_content_length = 0;
    m_host = nullptr;
    m_start_line_idx = 0;
    m_checking_idx = 0;
    m_read_last_next_idx = 0;
    m_write_needto_send = 0;

    m_mysql = nullptr;
    bytes_to_send = 0;
    bytes_have_send = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;
    memset(m_read_buf, 0, READ_BUFFER_SIZE);
    memset(m_write_buf, 0, WRITE_BUFFER_SIZE);
    memset(m_real_file_path, 0, FILENAME_LEN);
}

/*解析HTTP请求行，获得请求方法、目标URL，以及HTTP版本号*/
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    /*  const char *a = "abcd\t\ndef";
        const char *p;
        p = strpbrk(a, "\t");
        printf("%s\n", p);// "\t\ndef"
     */
    //截取method
    //    char *temp = new char[100];
    //    strcpy(temp, text);
    m_url = strpbrk(text, " \t");
    /*如果请求行中没有空白字符或"\t",则HTTP请求有问题*/
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = 0;

    char *url_method = text;
    //判断字符串是否相等(忽略大小写)
    if (strcasecmp(url_method, "GET") == 0)
    {
        m_method = GET;
    }
    else if (strcasecmp(url_method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }
    else
    {
        return BAD_REQUEST;
    }
    //检索字符串 str1 中第一个不在字符串 str2 中出现的字符下标
    //    std::string str1 = "abc defg";
    //    char *p = (char *)str1.c_str();
    //    auto out = strspn(p, "abc");
    //    char *t = p + out;
    //    cout << out << endl;//3
    //    cout << t << endl;  // " defg"
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
    {
        return BAD_REQUEST;
    }
    *m_version++ = 0;
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
    {
        return BAD_REQUEST;
    }
    //只比较前7个字符
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        //strchr() 用于查找字符串中的一个字符，并返回该字符在字符串中第一次出现的位置(char*)。
        m_url = strchr(m_url, '/');
    }

    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }
    if (!m_url || m_url[0] != '/')
    {
        return BAD_REQUEST;
    }
    //当url为'/'，显示判断界面，进入显示界面
    if (strlen(m_url) == 1)
    {
        strcat(m_url, "judge.html");
    }
    /*HTTP请求行处理完毕，状态转移到头部字段分析*/
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

/*解析一个HTTP请求的头部信息*/
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    /*遇到空行，表示头部字段解析完毕*/
    if (text[0] == '\0')
    {
        /*如果HTTP请求有消息体，则还需要读取content_length字节的消息体*/
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        /*否则我们看到一个完整请求*/
        return GET_REQUEST;
    }
    /*处理Connection字段*/
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    /*处理Content-Length头部字段*/
    else if (strncasecmp(text, "Content-Length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    /*处理Host字段*/
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {
        printf("oop! Unknow header %s\n", text);
    }
    return NO_REQUEST;
}

/*不解析HTTP请求的消息体，只是判断它是否被完整的读入了*/
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    if (m_read_last_next_idx >= (m_content_length + m_checking_idx))
    {
        text[m_content_length] = '\0';
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

/*根据服务器处理HTTP请求的结果，返回给客户端的内容*/
bool http_conn::process_write(http_conn::HTTP_CODE ret)
{
    switch (ret)
    {
        case INTERNAL_ERROR:
        {
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if (!add_content(error_500_form))
            {
                return false;
            }
            break;
        }
        case BAD_REQUEST:
        {
            add_status_line(400, error_400_title);
            add_headers(strlen(error_400_form));
            if (!add_content(error_400_form))
            {
                return false;
            }
            break;
        }
        case NO_RESOURCE:
        {
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if (!add_content(error_404_form))
            {
                return false;
            }
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            add_status_line(403, error_403_title);
            add_headers((int)strlen(error_403_form));
            if (!add_content(error_403_form))
            {
                return false;
            }
            break;
        }
        case FILE_REQUEST:
        {
            add_status_line(200, ok_200_title);
            if (m_file_stat.st_size != 0)
            {
                add_headers((int)m_file_stat.st_size);
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_needto_send;
                m_iv[1].iov_base = m_file_maddress;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                bytes_to_send = m_write_needto_send + (int)m_file_stat.st_size;
                return true;
            }
            else
            {
                const char *ok_string = "<html><body></body></html>";
                add_headers((int)strlen(ok_string));
                if (!add_content(ok_string))
                {
                    return false;
                }
            }
        }
        default:
            return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_needto_send;
    m_iv_count = 1;
    bytes_to_send = m_write_needto_send;
    return true;
}

/*当得到一个完整的、正确的HTTP请求时，我们就分析目标文件的属性。如果目标文件存在、对所有用户可读，
 * 且不是目录，则使用mmap将其映射到内存地址file_address处，并告诉调用者读取文件成功*/
http_conn::HTTP_CODE http_conn::do_request()
{
    strcpy(m_real_file_path, doc_root);
    int len = (int)strlen(doc_root);
    const char *p = strrchr(m_url, '/');
    if (cgi && (*(p + 1) == '2' || *(p + 1) == '3'))
    {
        //根据标志判断是登录检测还是注册检测
        char flag = m_url[1];
        char *m_url_real = (char *)malloc(200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file_path + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        //提取用户名和密码
        //user=xxx&passwd=xxx
        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
        {
            name[i - 5] = m_string[i];
        }
        name[i - 5] = '\0';
        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
        {
            password[j] = m_string[i];
        }
        password[j] = '\0';

        if (*(p + 1) == '3')
        {
            //注册
            //            char *sql_insert = (char *)malloc(200);
            //            strcpy(sql_insert, "INSERT INTO user(username,passwd) VALUES(");
            auto *sql = new string("INSERT INTO user(username,passwd) VALUES(");
            *sql += "'" + string(name) + "', '" + string(password) + "')";
            if (users.find(name) == users.end())
            {
                m_lock.lock();
                int res = mysql_query(m_mysql, (*sql).c_str());
                users.insert(pair<string, string>(name, password));
                m_lock.unlock();

                if (!res)
                {
                    strcpy(m_url, "/log.html");
                }
                else
                {
                    strcpy(m_url, "/registerError.html");
                }
            }
            else
                strcpy(m_url, "/registerError.html");
            delete sql;
        }
        //登录，直接判断
        else if (*(p + 1) == '2')
        {
            if (users.find(name) != users.end() && users[name] == password)
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/logError.html");
        }
    }
    //注册
    if (*(p + 1) == '0')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file_path + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    //登录
    else if (*(p + 1) == '1')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file_path + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    //图片
    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file_path + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    //视频
    else if (*(p + 1) == '6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/vedio.html");
        strncpy(m_real_file_path + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    //fans
    else if (*(p + 1) == '7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file_path + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else
    {
        strncpy(m_real_file_path + len, m_url, FILENAME_LEN - len - 1);
    }

    if (stat(m_real_file_path, &m_file_stat) < 0)
        return NO_RESOURCE;
    if (!(m_file_stat.st_mode & S_IROTH))
    {
        return FORBIDDEN_REQUEST;
    }
    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    int fd = open(m_real_file_path, O_RDONLY);
    m_file_maddress = (char *)mmap(nullptr, m_file_stat.st_size, PROT_READ,
                                   MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

void http_conn::unmap()
{
    if (m_file_maddress)
    {
        munmap(m_file_maddress, m_file_stat.st_size);
        m_file_maddress = nullptr;
    }
}

char *http_conn::get_line()
{
    return m_read_buf + m_start_line_idx;
}

/*从读缓存中解析一行*/
http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for (; m_checking_idx < m_read_last_next_idx; ++m_checking_idx)
    {
        temp = m_read_buf[m_checking_idx];
        if (temp == '\r')
        {
            if (m_checking_idx == m_read_last_next_idx - 1)
            {
                return LINE_OPEN;
            }
            else if (m_read_buf[m_checking_idx + 1] == '\n')
            {
                m_read_buf[m_checking_idx++] = 0;
                m_read_buf[m_checking_idx++] = 0;
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (m_checking_idx > 1 && m_read_buf[m_checking_idx - 1] == '\r')
            {
                m_read_buf[m_checking_idx - 1] = '\0';
                m_read_buf[m_checking_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

/*往写缓冲中写入待发送的数据*/
bool http_conn::add_response(const char *format, ...)
{
    if (m_write_needto_send >= WRITE_BUFFER_SIZE)
    {
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);
    //函数返回值：若缓冲区足够大，返回存入数组的字符数；若编码出错，返回负值
    int len = vsnprintf(m_write_buf + m_write_needto_send,
                        WRITE_BUFFER_SIZE - 1 - m_write_needto_send, format,
                        arg_list);
    if (len >= WRITE_BUFFER_SIZE - 1 - m_write_needto_send)
    {
        return false;
    }
    m_write_needto_send += len;
    va_end(arg_list);
    LOG_INFO("request:%s", m_write_buf)
    return true;
}

bool http_conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

void http_conn::add_headers(int contentlength)
{
    add_content_length(contentlength);
    add_linger();
    add_blank_line();
}

bool http_conn::add_content_length(int cont_length)
{
    return add_response("Content-Length: %d\r\n", cont_length);
}

bool http_conn::add_linger()
{
    return add_response("Connection: %s\r\n",
                        m_linger ? "keep-alive" : "close");
}

bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}

bool http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}

void http_conn::initmysql_result(connectpool *connPool)
{
    //先从连接池取得一个连接
    MYSQL *mysql_conn = nullptr;
    connectionRAII mysqlconnraii(&mysql_conn, connPool);
    //检索数据，浏览器输入
    const char *sql = "SELECT username,passwd FROM user";
    if (mysql_query(mysql_conn, sql))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql_conn))
    }
    //获取结果集
    MYSQL_RES *res = mysql_store_result(mysql_conn);
    //列数
    auto num_fields = mysql_num_fields(res);
    //所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_field(res);
    //获取下一行
    while (MYSQL_ROW row = mysql_fetch_row(res))
    {
        string tmp1 = row[0], tmp2 = row[1];
        users[tmp1] = tmp2;
    }
    //防止内存泄漏
    mysql_free_result(res);
}

bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}

void http_conn::process()
{
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sock_fd, EPOLLIN, m_TRIGMode);
        return;
    }

    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
    }
    modfd(m_epollfd, m_sock_fd, EPOLLOUT, m_TRIGMode);
}

/*主状态机，分析HTTP请求*/
http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret;
    char *text;
    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) ||
           ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        m_start_line_idx = m_checking_idx;
        //        printf("got 1 http line: %s\n", text);
        LOG_INFO("%s", text);
        switch (m_check_state)
        {
                /*分析请求行*/
            case CHECK_STATE_REQUESTLINE:
            {
                if (parse_request_line(text) == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                break;
            }
                /*分析头部字段*/
            case CHECK_STATE_HEADER:
            {
                ret = parse_headers(text);
                if (ret == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                else if (ret == GET_REQUEST)
                {
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT:
            {
                if (parse_content(text) == GET_REQUEST)
                {
                    return do_request();
                }
                line_status = LINE_OPEN;
                break;
            }

            default:
                return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}