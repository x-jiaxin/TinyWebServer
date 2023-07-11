//
// Created by jiaxin on 2023/6/25 13:39.
//

#ifndef TINYWEBSERVER_LST_TIMER_H
#define TINYWEBSERVER_LST_TIMER_H

#include <iostream>
#include <ctime>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <csignal>
#include <cassert>
#include <http_conn.h>

class util_timer;

/*用户数据结构*/
struct client_data {
    /*客户端socket地址*/
    sockaddr_in client_address;
    /*客户端socket文件描述符*/
    int sockfd;
    /*定时器类*/
    util_timer *timer;
};

/*定时器类*/
class util_timer
{
public:
    util_timer() : next(nullptr), prev(nullptr) {}

public:
    /*超时时间*/
    time_t expire{};
    /*任务回调函数，处理客户数据，由定时器执行者传给回调函数*/
    void (*cb_func)(client_data *);
    client_data *user_data{};
    util_timer *prev;
    util_timer *next;
};

/*定时器链表，升序、双向、带头尾节点*/
class sort_timer_lst
{
private:
    util_timer *head;
    util_timer *tail;

public:
    sort_timer_lst();
    /*循环delete整个链表*/
    ~sort_timer_lst();
    /*将目标定时器添加到链表中*/
    void add_timer(util_timer *timer);
    /*某个定时任务发生变化，调整整个链表。只考虑时间被延长的情况，即该定时器需要后移*/
    void adjust_timer(util_timer *timer);
    /*删除定时器*/
    void delete_timer(util_timer *timer);
    /*SIGALRM信号处理函数里调用该函数(如果是统一事件源，则是主函数)，用于处理链表到期任务*/
    void tick();

private:
    //将定时器添加到lst_head以后的链表中
    //找一个超时时间大于定时器的结点，插入到其之前
    void add_timer(util_timer *timer, util_timer *lst_head);
};

class Utils
{
public:
    int TIMESLOT{};
    static int *u_pipefd;
    static int u_epollfd;
    sort_timer_lst m_time_list;

public:
    Utils() = default;
    ~Utils() = default;

    void init(int timeslot = 5);

    int setnonblocking(int fd);

    void addfd(int epollfd, int fd, bool one_shot, int trig_mode);

    static void sig_handler(int sig);

    void addsig(int sig, void(handler)(int), bool restart = true);

    void timer_handler();

    void show_error(int connfd, const char *info);
};
void cb_func(client_data *user_data);
#endif//TINYWEBSERVER_LST_TIMER_H
