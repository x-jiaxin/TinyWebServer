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
    threadpool(int actor_model, connectpool *conn_pool, int thread_number = 8,
               int max_request = 10000);
    ~threadpool();
    bool append(T *request, int state);
    bool append_p(T *request);

private:
    /*工作线程运行的函数，从工作队列中取出任务并执行*/
    static void *worker(void *arg);
    [[noreturn]] void run();

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
    : m_thread_number(thread_number), m_max_requests(max_request),
      m_threads(nullptr), m_conn_pool(conn_pool), m_actor_model(actor_model)
{
    if (thread_number <= 0 || max_request <= 0) {
        throw std::runtime_error("thread_number max_request <= 0!");
    }
    m_threads = new pthread_t[thread_number];
    if (!m_threads) {
        throw std::runtime_error("m_threads fail!");
    }
    for (int i = 0; i < m_thread_number; ++i) {
        if (pthread_create(m_threads + i, nullptr, worker, this) != 0) {
            delete[] m_threads;
            throw std::runtime_error("pthread_create fail!");
        }
        if (pthread_detach(m_threads[i]) != 0) {
            delete[] m_threads;
            throw std::runtime_error("pthread_detach fail!");
        }
    }
}

template<typename T>
bool threadpool<T>::append(T *request, int state)
{
    m_queue_lock.lock();
    if (m_work_queue.size() > m_max_requests) {
        m_queue_lock.unlock();
        return false;
    }

    m_work_queue.push_back(request);
    request->m_state = state;
    m_queue_stat.post();
    m_queue_lock.unlock();
    return true;
}

template<typename T>
bool threadpool<T>::append_p(T *request)
{
    m_queue_lock.lock();
    if (m_work_queue.size() > m_max_requests) {
        m_queue_lock.unlock();
        return false;
    }

    m_work_queue.push_back(request);
    m_queue_stat.post();
    m_queue_lock.unlock();
    return true;
}

/*静态函数*/
template<typename T>
void *threadpool<T>::worker(void *arg)
{
    auto pool = (threadpool<T> *)arg;
    pool->run();
    return pool;
}

/*从工作队列中取任务。进行处理*/
template<typename T>
void threadpool<T>::run()
{
    while (true) {
        m_queue_stat.wait();
        m_queue_lock.lock();
        if (m_work_queue.empty()) {
            m_queue_lock.unlock();
            continue;
        }
        T *request = m_work_queue.front();
        m_work_queue.pop_front();
        m_queue_lock.unlock();
        if (!request) {
            continue;
        }
        if (m_actor_model == 1) {
            /*reactor*/
            if (request->m_state == 0) {
                //读
                if (request->read_once()) {
                    request->improv = 1;
                    connectionRAII conn(&request->m_mysql, m_conn_pool);
                    request->process();
                }
                else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else {
                //写
                if (request->write()) {
                    request->improv = 1;
                }
                else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else {
            /*proactor*/
            connectionRAII conn(&request->m_mysql, m_conn_pool);
            request->process();
        }
    }
}
#endif//TINYWEBSERVER_THREADPOOL_H
