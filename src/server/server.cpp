//
// Created by jiaxin on 2023/6/25 19:34.
//

#include "server.h"
server::server()
{
    users = new http_conn[MAX_FD];
    user_timer = new client_data[MAX_FD];

    char path[200];
    strcpy(m_root, getcwd(path, 200));
    char root[6] = "/root";
    strcat(m_root, root);
}

server::~server()
{
    delete[] user_timer;
    delete[] users;
    delete m_thread_pool;
    close(m_listen_fd);
    close(m_epollfd);
    close(m_pipefd[0]);
    close(m_pipefd[1]);
}

void server::init(int port, const string &user, const string &passwd,
                  const string &dbname, int log_write, int linger, int trigmode,
                  int sqlnum, int threadnum, int closelog, int actormodel)
{
    m_port = port;
    m_sql_username = user;
    m_sql_passwd = passwd;
    m_sql_dbname = dbname;
    m_log_write = log_write;
    m_linger = linger;
    m_trigmode = trigmode;
    m_sql_num = sqlnum;
    m_thread_num = threadnum;
    m_close_log = closelog;
    m_actmodel = actormodel;
}

void server::thread_pool()
{
    m_thread_pool =
            new threadpool<http_conn>(m_actmodel, m_sql_pool, m_thread_num);
}

void server::sql_pool()
{
    m_sql_pool = connectpool::GetInstance();
    m_sql_pool->init("localhost", m_sql_username, m_sql_passwd, m_sql_dbname,
                     3306, m_sql_num, m_close_log);

    users->initmysql_result(m_sql_pool);
}

void server::log_write()
{
    if (m_close_log == 0) {
        if (m_log_write) {
            /*异步日志*/
            Log::getInstance()->init("./log", m_close_log, 2000, 800000, 800);
        }
        else {
            Log::getInstance()->init("./log", m_close_log, 2000, 800000, 0);
        }
    }
}

void server::trig_mode()
{
    /*LT + LT*/
    if (m_trigmode == 0) {
        m_listentrigmode = 0;
        m_conntrigmode = 0;
    }
    /*LT + ET*/
    else if (m_trigmode == 1) {
        m_listentrigmode = 0;
        m_conntrigmode = 1;
    }
    /*ET + LT*/
    else if (m_trigmode == 2) {
        m_listentrigmode = 1;
        m_conntrigmode = 0;
    }
    /*ET + ET*/
    else if (m_trigmode == 3) {
        m_listentrigmode = 1;
        m_conntrigmode = 1;
    }
}

void server::eventListen()
{
    m_listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listen_fd >= 0);

    /*关闭连接*/
    if (0 == m_linger) {
        //        close()立刻返回，底层会将未发送完的数据发送完成后再释放资源，即优雅退出。
        linger tmp = {0, 1};
        setsockopt(m_listen_fd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else {
        linger tmp = {1, 1};
        setsockopt(m_listen_fd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    //复用socket地址
    int flag = 1;
    setsockopt(m_listen_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listen_fd, (sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listen_fd, 5);
    assert(ret >= 0);

    utils.init(TIMESLOT);
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    utils.addfd(m_epollfd, m_listen_fd, false, m_listentrigmode);
    http_conn::m_epollfd = m_epollfd;

    /*统一事件源*/
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    utils.setnonblocking(m_pipefd[1]);
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);

    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM, Utils::sig_handler, false);
    utils.addsig(SIGTERM, Utils::sig_handler, false);
    alarm(TIMESLOT);
}

void server::timer(int connfd, sockaddr_in client_address)
{
    users[connfd].init(connfd, client_address, m_root, m_conntrigmode,
                       m_close_log, m_sql_username, m_sql_passwd, m_sql_dbname);
    //初始化client_data数据
    //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    user_timer[connfd].client_address = client_address;
    user_timer[connfd].sockfd = connfd;
    auto *timer = new util_timer;
    timer->user_data = &user_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(nullptr);
    timer->expire = cur + 3 * TIMESLOT;
    user_timer[connfd].timer = timer;
    utils.m_time_list.add_timer(timer);
}

//若有数据传输，则将定时器往后延迟3个单位
//并对新的定时器在链表上的位置进行调整
void server::adjust_timer(util_timer *timer)
{
    time_t cur = time(nullptr);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_time_list.adjust_timer(timer);
    LOG_INFO("%s", "adjust timer once")
}

void server::deal_timer(util_timer *timer, int sockfd)
{
    timer->cb_func(&user_timer[sockfd]);
    if (timer) {
        utils.m_time_list.delete_timer(timer);
    }
    LOG_INFO("close fd %d", user_timer[sockfd].sockfd)
}

bool server::dealclientdata()
{
    return false;
}
bool server::dealwithsignal(bool &timeout, bool &stop_server)
{
    return false;
}
void server::dealwithread(int sockfd) {}

void server::dealwithwrite(int sockfd) {}

void server::eventLoop() {}
