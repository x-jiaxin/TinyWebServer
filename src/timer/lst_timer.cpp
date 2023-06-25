//
// Created by jiaxin on 2023/6/25 13:39.
//

#include <fcntl.h>
#include <cstring>
#include "lst_timer.h"
sort_timer_lst::sort_timer_lst() : head(nullptr), tail(nullptr) {}

sort_timer_lst::~sort_timer_lst()
{
    auto p = head;
    while (p) {
        head = p->next;
        delete p;
        p = head;
    }
}

void sort_timer_lst::add_timer(util_timer *timer)
{
    if (!timer) {
        return;
    }
    if (!head) {
        head = timer;
        return;
    }
    //链表升序
    //小于头节点时间
    if (timer->expire < head->expire) {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }

    //找合适的位置
    add_timer(timer, head);
}

void sort_timer_lst::adjust_timer(util_timer *timer)
{
    if (!timer) {
        return;
    }
    util_timer *tmp = timer->next;
    //1. 在尾部或者定时器值小于下一个定时器，则不用调整
    if (!tmp || timer->expire < tmp->expire) {
        return;
    }
    //2. 头结点，取出后重新插入链表
    if (timer == head) {
        head = head->next;
        head->prev = nullptr;
        timer->next = nullptr;
        add_timer(timer, head);
    }//3.
    else {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, head);
    }
}

void sort_timer_lst::delete_timer(util_timer *timer)
{
    if (!timer) {
        return;
    }
    //1.只有一个节点
    if (timer == head && timer == tail) {
        delete timer;
        head = nullptr;
        tail = nullptr;
        return;
    }
    //2.删除节点是头结点
    if (timer == head) {
        head = timer->next;
        head->prev = nullptr;
        delete timer;
        return;
    }
    //3.删除的是尾结点
    if (timer == tail) {
        tail = timer->prev;
        tail->next = nullptr;
        delete timer;
        return;
    }
    //4.删除的是中间结点
    timer->next->prev = timer->prev;
    timer->prev->next = timer->next;
    delete timer;
}

void sort_timer_lst::tick()
{
    if (!head) {
        return;
    }
    printf("timer tick!\n");
    auto cur_time = time(nullptr);
    util_timer *tmp = head;
    //从头开始处理每个定时器，直到遇到一个尚未到期的定时器
    while (tmp) {
        //每个定时器使用了绝对时间作为超时值，可以用当前系统时间判断定时器是否到期
        if (cur_time < tmp->expire) {
            break;
        }
        //定时器的回调函数
        tmp->cb_func(tmp->user_data);
        //执行完定时器的定时任务后，将该定时器删除
        head = tmp->next;
        //
        if (head) {
            head->prev = nullptr;
        }
        delete tmp;
        tmp = head;
    }
}

void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head)
{
    util_timer *pre = lst_head, *tmp = lst_head->next;
    while (tmp) {
        if (tmp->expire > timer->expire) {
            pre->next = timer;
            timer->prev = pre;
            timer->next = tmp;
            tmp->prev = timer;
            break;
        }
        pre = tmp;
        tmp = tmp->next;
    }
    //到了尾结点仍未找到合适的位置，则成为新的尾结点
    if (!tmp) {
        pre->next = timer;
        timer->prev = pre;
        timer->next = nullptr;
        tail = timer;
    }
}

void Utils::init(int timeslot)
{
    TIMESLOT = timeslot;
}

int Utils::setnonblocking(int fd)
{
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opt);
    return old_opt;
}

void Utils::addfd(int epollfd, int fd, bool one_shot, int trig_mode)
{
    epoll_event event{};
    event.data.fd = fd;
    if (trig_mode) {
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }
    else {
        event.events = EPOLLIN | EPOLLRDHUP;
    }
    if (one_shot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void Utils::sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

void Utils::addsig(int sig, void (*handler)(int), bool restart)
{
    struct sigaction sa {};
    sa.sa_handler = handler;
    if (restart) {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, nullptr) != -1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    m_time_list.tick();
    alarm(TIMESLOT);
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = nullptr;
int Utils::u_epollfd = 0;

void cb_func(client_data *user_data)
{
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    --http_conn::m_user_count;
}