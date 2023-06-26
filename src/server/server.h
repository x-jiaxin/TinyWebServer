//
// Created by jiaxin on 2023/6/25 19:34.
//

#ifndef TINYWEBSERVER_SERVER_H
#define TINYWEBSERVER_SERVER_H

#include "http_conn.h"
#include "threadpool.h"
#include "lst_timer.h"

const int MAX_FD = 2 << 15;        //最大文件描述符
const int MAX_EVENT_NUMBER = 10000;//最大事件数
const int TIMESLOT = 5;            //最小超时单位
class server
{
public:
    server();
    ~server();

    void init(int port, const string &user, const string &passwd,
              const string &dbname, int log_write, int linger, int trigmode,
              int sqlnum, int threadnum, int closelog, int actormodel);

    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();
    void eventListen();
    void eventLoop();
    void timer(int connfd, const sockaddr_in &client_address);
    void adjust_timer(util_timer *timer);
    void deal_timer(util_timer *timer, int sockfd);
    bool deal_clientdata();
    bool deal_signal(bool &timeout, bool &stop_server);
    void deal_read(int sockfd);
    void deal_write(int sockfd);

public:
    int m_port;
    char *m_root;
    int m_log_write;
    int m_close_log;
    int m_actmodel;

    int m_pipefd[2];
    int m_epollfd;
    http_conn *users;

    connectpool *m_sql_pool;
    string m_sql_username;
    string m_sql_passwd;
    string m_sql_dbname;
    int m_sql_num;

    threadpool<http_conn> *m_thread_pool;
    int m_thread_num;

    epoll_event events[MAX_EVENT_NUMBER];

    int m_listen_fd;
    int m_linger;
    int m_trigmode;
    int m_listentrigmode;
    int m_conntrigmode;

    client_data *m_user_data;
    Utils utils;
};

#endif//TINYWEBSERVER_SERVER_H
