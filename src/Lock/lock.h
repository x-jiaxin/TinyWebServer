//
// Created by jiaxin on 2023/6/21 13:58.
//

#ifndef TINYWEBSERVER_LOCK_H
#define TINYWEBSERVER_LOCK_H

#include <exception>
#include <pthread.h>
#include <sys/time.h>
#include <semaphore.h>

class sem
{
public:
    sem()
    {
        if (sem_init(&m_sem, 0, 0) != 0)
        {
            throw std::exception();
        }
    }
    explicit sem(int num)
    {
        if (sem_init(&m_sem, 0, num) != 0)
        {
            throw std::exception();
        }
    }
    ~sem()
    {
        sem_destroy(&m_sem);
    }
    bool wait()
    {
        return sem_wait(&m_sem) == 0;
    }
    bool post()
    {
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem{};
};

class locker
{
public:
    locker()
    {
        if (pthread_mutex_init(&m_mutex, nullptr) != 0)
        {
            throw std::exception();
        }
    }
    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    pthread_mutex_t *get()
    {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};

class cond
{
public:
    cond()
    {
        /*        if (pthread_mutex_init(&m_mutex, nullptr) != 0) {
            throw std::exception();
        }*/
        if (pthread_cond_init(&m_cond, nullptr) != 0)
        {
            //            pthread_mutex_destroy(&m_mutex);
            throw std::exception();
        }
    }
    ~cond()
    {
        //        pthread_mutex_destroy(&m_mutex);
        pthread_cond_destroy(&m_cond);
    }

    bool wait(pthread_mutex_t *mutex)
    {
        int ret;
        //        pthread_mutex_lock(&m_mutex);
        //        ret = pthread_cond_wait(&m_cond, mutex);

        ret = pthread_cond_wait(&m_cond, mutex);
        //        pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }

    //唤醒一个等待条件变量的线程
    bool signal()
    {
        return pthread_cond_signal(&m_cond) == 0;
    }

    //广播唤醒等待条件变量的线程
    bool broadcast()
    {
        return pthread_cond_broadcast(&m_cond) == 0;
    }

    bool timewait(pthread_mutex_t *mutex, struct timespec &t)
    {
        return pthread_cond_timedwait(&m_cond, mutex, &t) == 0;
    }

private:
    //    pthread_mutex_t m_mutex{};
    pthread_cond_t m_cond{};
};
#endif//TINYWEBSERVER_LOCK_H