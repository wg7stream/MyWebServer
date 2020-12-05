#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>//  stdarg.h是C语言中C标准函数库的头文件,stdarg是由standard(标准) arguments(参数)简化而来,主要目的为让函数能够接收可变参数
#include <pthread.h>
#include "block_queue.h"

using namespace std;

class Log{
public:
    // C++11以后,使用局部变量懒汉不用加锁
    // 使用函数内的局部静态对象，这种方法不用加锁和解锁操作
    static Log *get_instance(){// 公有的实例获取方法
        static Log instance;
        return &instance;
    }

    // 初始化日志
    // 该函数会创建好日志文件的名字，并以该名字打开一个文件
    bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

    // 异步写日志
    static void *flush_log_thread(void *args){
        Log::get_instance()->async_write_log();
    }

    void write_log(int level, const char *format, ...);

    void flush(void);

private:
    Log();
    virtual ~Log();

    void *async_write_log(){
        string single_log;

        while (m_log_queue->pop(single_log)){
            m_mutex.lock();
            // 将该条日志写到文件中
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }
    }

private:
    char dir_name[128];
    char log_name[128];
    int m_split_lines;
    int m_log_buf_size;
    long long m_count;
    int m_today;
    FILE *m_fp;
    char *m_buf;
    block_queue<string> *m_log_queue;
    bool m_is_async;
    locker m_mutex;
    int m_close_log;
};


#define LOG_DEBUG(format, ...) if (0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_LOGINFO(format, ...) if (0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if (0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if (0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}


#endif