//
// Created by jiaxin on 2023/6/22 16:30.
//

#ifndef TINYWEBSERVER_BLOCK_QUEUE_H
#define TINYWEBSERVER_BLOCK_QUEUE_H

#include <iostream>
#include "lock.h"
#include <cassert>
using std::cin;
using std::cout;
using std::endl;
/**循环数组实现的阻塞队列，m_back = (m_back + 1) % m_max_size;
 * \n 线程安全
 */
template<typename T>
class block_queue
{
public:
    explicit block_queue(int maxsize = 1000) : m_max_size(maxsize)
    {
        assert(m_max_size > 0);
        m_arr = new T[m_max_size];
        m_cur_size = 0;
        m_front = -1;
        m_back = -1;
    }

    ~block_queue()
    {
        m_mutex.lock();

        if (!m_arr) {
            delete[] m_arr;
        }
        m_mutex.unlock();
    }

    void clear()
    {
        m_mutex.lock();
        m_cur_size = 0;
        m_front = -1;
        m_back = -1;
        m_mutex.unlock();
    }

    bool full()
    {
        m_mutex.lock();
        if (m_cur_size >= m_max_size) {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    bool empty()
    {
        m_mutex.lock();
        if (m_cur_size == 0) {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    void front(T &elem)
    {
        m_mutex.lock();
        if (m_cur_size == 0) {
            m_mutex.unlock();
            return;
        }
        elem = m_arr[m_front];
        m_mutex.unlock();
    }

    void back(T &elem)
    {
        m_mutex.lock();
        if (m_cur_size == 0) {
            m_mutex.unlock();
            return;
        }
        elem = m_arr[m_back];
        m_mutex.unlock();
    }

    int size()
    {
        m_mutex.lock();
        int tmp = this->m_cur_size;
        m_mutex.unlock();
        return tmp;
    }

    int MaxSize()
    {
        m_mutex.lock();
        int tmp = this->m_max_size;
        m_mutex.unlock();
        return tmp;
    }

    /*当有元素进队列时，生产者生产了一个元素，唤醒所有线程*/
    bool push(const T &item)
    {
        m_mutex.lock();
        if (m_cur_size >= m_max_size) {
            m_condation.broadcast();
            m_mutex.unlock();
            return false;
        }

        m_back = (m_back + 1) % m_max_size;
        m_arr[m_back] = item;
        ++m_cur_size;

        m_condation.broadcast();
        m_mutex.unlock();
        return true;
    }

    //pop时,如果当前队列没有元素,将会等待条件变量
    bool pop(T &elem)
    {
        m_mutex.lock();
        while (m_cur_size <= 0) {
            /* timeval now{};
            gettimeofday(&now, nullptr);
            timespec s{now.tv_sec + 5, now.tv_usec * 1000};
            if (!m_condation.timewait(m_mutex.get(), s)) {*/

            // m_condation.wait()前，互斥锁必须已上锁
            if (!m_condation.wait(m_mutex.get())) {
                printf("!!\n");
                m_mutex.unlock();
                return false;
            }
        }

        m_front = (m_front + 1) % m_max_size;
        elem = m_arr[m_front];
        --m_cur_size;
        m_mutex.unlock();
        return true;
    }

private:
    locker m_mutex;
    cond m_condation;

    T *m_arr;
    int m_cur_size;
    int m_max_size;
    /*循环数组的队头队尾*/
    int m_front;
    int m_back;
};

#endif//TINYWEBSERVER_BLOCK_QUEUE_H
