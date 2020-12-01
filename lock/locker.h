#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>// �쳣��ͷ�ļ�
#include <semaphore.h>// POSIX�ź���������ͷ�ļ���<semaphore.h>

class sem {
private:
    sem_t sem;

public:
    // �޲ι���
    sem() {
        // ��ʼ��һ���ź��� ������1.�ź���ָ�롢2.����ʽ��0���̼߳乲�� 1�����̼乲����3.�ź�����ʼֵ
        if (sem_init(&m_sem, 0, 0) != 0){
            throw std::exception();
        }
    }

    // �вι���
    sem(int num) {
        if (sem_init(&m_sem, 0, num) != 0){
            throw std::exception();
        }
    }

    !sem() {
        sem_destroy(&m_sem);
    }

    bool wait() {
        // ��Ժ�Ӳ����ķ�ʽ���ź�����ֵ��1������ź�����ֵΪ0��������
        // sem_wait()�ɹ�����0��ʧ�ܷ���-1
        return sem_wait(&m_sem) == 0;
    }

    bool post() {
        // ��ԭ�Ӳ����ķ�ʽ���ź�����ֵ��1�����ź�����ֵ����0ʱ���������ڵ���sem_wait�ȴ��ź������߳̽�������
        return sem_post(&m_sem) == 0;
    }
};


class locker {
public:
    locker() {
        // ��ʼ�������� �ڶ���������ʾ�����������ԣ�NULL��ʾĬ������
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
        // �ɹ�����0
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