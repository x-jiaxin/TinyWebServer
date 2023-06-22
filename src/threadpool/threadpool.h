//
// Created by jiaxin on 2023/6/21 13:40.
//

#ifndef TINYWEBSERVER_THREADPOOL_H
#define TINYWEBSERVER_THREADPOOL_H

#include <iostream>
#include "lock.h"
#include "connectpool.h"
using std::cin;
using std::cout;
using std::endl;

/**
 * 半同步/半反应堆线程池\n
 * 使用一个工作队列完全解除了主线程和工作线程的耦合关系：主线程往工作队列中插入任务，工作线程通过竞争来取得任务并执行它。\n
 * 同步I/O模拟proactor模式\n
 * 半同步/半反应堆\n
 * 线程池
 */
template<typename T>
class threadpool
{
public:
    threadpool() = default;
    threadpool(int actor_model, connectpool *conn_pool, int thread_number = 8,
               int max_request = 10000);
    ~threadpool();
    bool append(T *request, int state);
    bool append_p(T *request);

private:
    /*工作线程运行的函数，从工作队列中取出任务并执行*/
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;
    int m_max_requests;
    pthread_t *m_threads;
    list<T *> m_work_queue;
    locker m_queue_lock;
    sem m_queue_stat;
    connectpool *m_conn_pool;
    int m_actor_model;
};

template<typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

template<typename T>
threadpool<T>::threadpool(int actor_model, connectpool *conn_pool,
                          int thread_number, int max_request)
{
    if (thread_number <= 0 || max_request <= 0) {
        throw std::runtime_error("thread_number max_request <= 0!");
    }
    m_thread_number = thread_number;
    m_max_requests = max_request;
    m_threads = new pthread_t[thread_number];
    if (!m_threads) {
        throw std::runtime_error("m_threads fail!");
    }
}

template<typename T>
bool threadpool<T>::append(T *request, int state)
{
    return false;
}

template<typename T>
bool threadpool<T>::append_p(T *request)
{
    return false;
}

template<typename T>
void *threadpool<T>::worker(void *arg)
{
    return nullptr;
}

template<typename T>
void threadpool<T>::run()
{}
#endif//TINYWEBSERVER_THREADPOOL_H
