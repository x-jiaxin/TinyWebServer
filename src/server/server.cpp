//
// Created by jiaxin on 2023/6/25 19:34.
//

#include "server.h"
server::server()
{
    users = new http_conn[MAX_FD];
    m_user_data = new client_data[MAX_FD];

    chdir("/home/jiaxin/Code/TinyWebServer");
    char path[200];
    getcwd(path, 200);
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(path) + strlen(root) + 1);
    //    m_root = new char[strlen(path) + strlen(root) + 1];
    strcpy(m_root, path);
    strcat(m_root, root);
    //    printf("%s\n", m_root);
}

server::~server()
{
    delete[] m_user_data;
    delete[] users;
    delete m_thread_pool;
    delete m_root;
    //    free(m_root);
    close(m_listen_fd);
    close(m_epollfd);
    close(m_pipefd[0]);
    close(m_pipefd[1]);
}

void server::init(int port, const string &user, const string &passwd,
                  const string &dbname, int asyn_log_write, int linger,
                  int trigmode, int sqlnum, int threadnum, int closelog,
                  int actormodel)
{
    m_port = port;
    m_sql_username = user;
    m_sql_passwd = passwd;
    m_sql_dbname = dbname;
    m_asyn_log_write = asyn_log_write;
    m_linger = linger;
    m_trigmode = trigmode;
    m_sql_num = sqlnum;
    m_thread_num = threadnum;
    m_close_log = closelog;
    m_reactor_model = actormodel;
}

void server::thread_pool()
{
    m_thread_pool = new threadpool<http_conn>(m_reactor_model, m_sql_pool,
                                              m_thread_num);
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
    if (m_close_log == 0)
    {
        auto log_path = "/home/jiaxin/Code/TinyWebServer/log/.log";
        if (m_asyn_log_write)
        {
            /*异步日志*/
            Log::getInstance()->init(log_path, m_close_log, 2000, 800000, 800);
        }
        else
        {
            Log::getInstance()->init(log_path, m_close_log, 2000, 800000, 0);
        }
    }
}

void server::trig_mode()
{
    /*LT + LT*/
    if (m_trigmode == 0)
    {
        m_listenET = 0;
        m_connET = 0;
    }
    /*LT + ET*/
    else if (m_trigmode == 1)
    {
        m_listenET = 0;
        m_connET = 1;
    }
    /*ET + LT*/
    else if (m_trigmode == 2)
    {
        m_listenET = 1;
        m_connET = 0;
    }
    /*ET + ET*/
    else if (m_trigmode == 3)
    {
        m_listenET = 1;
        m_connET = 1;
    }
}

void server::eventListen()
{
    m_listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listen_fd >= 0);

    /*关闭连接*/
    if (0 == m_linger)
    {
        /*❑l_onoff等于0。此时SO_LINGER选项不起作用，close用默认行为
        来关闭socket。*/
        linger tmp = {0, 1};
        setsockopt(m_listen_fd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else
    {
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

    utils.addfd(m_epollfd, m_listen_fd, false, m_listenET);
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

/*创建定时器*/
void server::timer(int connfd, const sockaddr_in &client_address)
{
    users[connfd].init(connfd, client_address, m_root, m_connET, m_close_log,
                       m_sql_username, m_sql_passwd, m_sql_dbname);
    //初始化client_data数据
    //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    m_user_data[connfd].client_address = client_address;
    m_user_data[connfd].sockfd = connfd;
    auto *timer = new util_timer;
    timer->user_data = &m_user_data[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(nullptr);
    timer->expire = cur + 3 * TIMESLOT;
    m_user_data[connfd].timer = timer;
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

/*时间到，处理定时任务*/
void server::deal_timer(util_timer *timer, int sockfd)
{
    if (timer)
    {
        timer->cb_func(&m_user_data[sockfd]);
        utils.m_time_list.delete_timer(timer);
        LOG_INFO("close fd %d", m_user_data[sockfd].sockfd)
    }
    else
    {
        LOG_ERROR("timer is nullptr!")
    }
}

/*新连接*/
bool server::deal_clientdata()
{
    sockaddr_in client_address{};
    socklen_t sz = sizeof(client_address);
    if (0 == m_listenET)
    {
        //LT监听
        int connfd = accept(m_listen_fd, (sockaddr *)&client_address, &sz);
        if (connfd < 0)
        {
            LOG_ERROR("accept error! errno is %d", errno)
            return false;
        }
        if (http_conn::m_user_count >= MAX_FD)
        {
            utils.show_error(connfd, "Busy! Too many request");
            LOG_ERROR("Server Busy")
            return false;
        }
        timer(connfd, client_address);
    }
    else
    {
        //ET
        while (true)
        {
            int connfd = accept(m_listen_fd, (sockaddr *)&client_address, &sz);
            if (connfd < 0)
            {
                LOG_ERROR("accept error! errno is %d", errno)
                break;
            }
            if (http_conn::m_user_count >= MAX_FD)
            {
                utils.show_error(connfd, "Busy! Too many request");
                LOG_ERROR("Server Busy")
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

/*通过统一事件源从管道获得的信号值*/
bool server::deal_signal(bool &timeout, bool &stop_server)
{
    char signals[1024];
    int ret = (int)recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret <= 0)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < ret; ++i)
        {
            switch (signals[i])
            {
                case SIGALRM:
                {
                    /*超时信号*/
                    timeout = true;
                    break;
                }
                case SIGTERM:
                {
                    /*终止信号*/
                    stop_server = true;
                    break;
                }
            }
        }
    }
    return true;
}

void server::deal_read(int sockfd)
{
    auto timer = m_user_data[sockfd].timer;
    /*reactor，主线程不处理读*/
    if (m_reactor_model)
    {
        if (timer)
        {
            adjust_timer(timer);
        }
        /*读0，写1*/
        m_thread_pool->append(users + sockfd, 0);

        while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    /*proactor，主线程处理读*/
    else
    {
        if (users[sockfd].read_once())
        {
            LOG_INFO("deal with the client(%s)",
                     inet_ntoa(users[sockfd].getaddress()->sin_addr))
            m_thread_pool->append(users + sockfd, 0);
            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void server::deal_write(int sockfd)
{
    auto timer = m_user_data[sockfd].timer;
    /*reactor，主线程不处理读*/
    if (m_reactor_model)
    {
        if (timer)
        {
            adjust_timer(timer);
        }
        /*读0，写1*/
        m_thread_pool->append(users + sockfd, 1);

        while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    /*proactor，主线程处理写*/
    else
    {
        if (users[sockfd].write())
        {
            LOG_INFO("send data to the client(%s)",
                     inet_ntoa(users[sockfd].getaddress()->sin_addr))
            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void server::eventLoop()
{
    bool stop_server = false, time_out = false;
    while (!stop_server)
    {
        int number = epoll_wait(m_epollfd, m_events, MAX_EVENT_NUMBER, -1);

        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; ++i)
        {
            int eventfd = m_events[i].data.fd;
            /*新的连接*/
            if (eventfd == m_listen_fd)
            {
                if (!deal_clientdata())
                {
                    continue;
                }
            }
            /*服务器端关闭连接，移除对应的定时器*/
            else if (m_events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                util_timer *timer = m_user_data[eventfd].timer;
                deal_timer(timer, eventfd);
            }
            /*处理信号*/
            else if (eventfd == m_pipefd[0] && m_events[i].events & EPOLLIN)
            {
                if (!deal_signal(time_out, stop_server))
                {
                    LOG_ERROR("deal clientdata(signal) error")
                }
            }
            /*处理读*/
            else if (m_events[i].events & EPOLLIN)
            {
                deal_read(eventfd);
            }
            /*处理写*/
            else if (m_events[i].events & EPOLLOUT)
            {
                deal_write(eventfd);
            }
        }
        if (time_out)
        {
            utils.timer_handler();

            LOG_INFO("%s", "timer tick");

            time_out = false;
        }
    }
}
