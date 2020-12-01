#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>// 异常的头文件
#include <semaphore.h>// POSIX信号量的引用头文件是<semaphore.h>

class sem {
private:
    sem_t sem;

public:
    // 无参构造
    sem() {
        // 初始化一个信号量 参数：1.信号量指针、2.共享方式（0：线程间共享 1：进程间共享）、3.信号量初始值
        if (sem_init(&m_sem, 0, 0) != 0){
            throw std::exception();
        }
    }

    // 有参构造
    sem(int num) {
        if (sem_init(&m_sem, 0, num) != 0){
            throw std::exception();
        }
    }

    !sem() {
        sem_destroy(&m_sem);
    }

    bool wait() {
        // 以院子操作的方式将信号量的值减1，如果信号量的值为0，则阻塞
        // sem_wait()成功返回0，失败返回-1
        return sem_wait(&m_sem) == 0;
    }

    bool post() {
        // 以原子操作的方式将信号量的值加1，当信号量的值大于0时，其他正在调用sem_wait等待信号量的线程将被唤醒
        return sem_post(&m_sem) == 0;
    }
};


class locker {
public:
    locker() {
        // 初始化互斥锁 第二个参数表示互斥锁的属性，NULL表示默认属性
        if (pthread_mutex_init(&m_mutex, NULL) != 0){
            throw std::exception();
        }
    }

    ~locker() {
        pthread_mutex_destroy(&m_mutex);
    }

    bool lock(){
        return pthread_mutex_lock(&m_mutex) == 0;
    }

    bool unlock(){
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    pthread_mutex_t *get(){
        return &m_mutex;
    }
private:
    pthread_mutex_t m_mutex;
};


class cond {
private:
    pthread_cond_t m_cond;
public:
    cond(){
        if (pthread_cond_init(&m_cond, NULL) != 0){
            throw std::exception();
        }
    }
    ~cond() {
        pthread_cond_destroy(&m_cond);
    }

    bool wait(pthread_mutex_t *m_mutex){
        int ret = 0;
        // 成功返回0
        ret = pthread_cond_wait(&m_cond, m_mutex);
        return ret == 0;
    }

    bool timewait(pthread_mutex_t *m_mutex, struct timespec t){
        int ret = 0;

        ret = pthread_cond_timewait(&m_cond, m _mutex, &t);

        return ret == 0;
    }

    bool signal(){
        return pthread_cond_signal(&m_cond) == 0;
    }
    bool broadcast() {
        return pthread_cond_broadcast(&m_cond) == 0;
    }
};


#endif