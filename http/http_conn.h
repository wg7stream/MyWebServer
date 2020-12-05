#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <unistd.h>// unistd.h是unix std的意思，是POSIX标准定义的unix类系统定义符号常量的头文件，
                   // 包含了许多UNIX系统服务的函数原型，例如read函数、write函数和getpid函数。 
#include <signal.h>
#include <sys/types.h>// 是Unix/Linux系统的基本系统数据类型的头文件，含有size_t，time_t，pid_t等类型
#include <sys/epoll.h>
#include <fcntl.h>// 是unix标准中通用的头文件，其中包含的相关函数有 open，fcntl，shutdown，unlink，fclose等！
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>// 是unix/linux系统定义文件状态所在的伪标准头文件。
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>// stdlib 头文件里包含了C、C++语言的最常用的系统函数 　　该文件包含了的C语言标准库函数的定义
#include <sys/mman.h>
#include <stdarg.h>// stdarg.h是C语言中C标准函数库的头文件，stdarg是由standard（标准） arguments（参数）简化而来，主要目的为让函数能够接收可变参数
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>

#include "../lock/locker.h"
#include "../Timer/timer.h"


class http_conn{
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;

    // 请求方法
    enum METHOD{
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };

    // 主状态机状态
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,// 解析请求行
        CHECK_STATE_HEADER,// 解析请求头
        CHECK_STATE_CONTENT// 解析消息体 仅用于解析POST请求
    };
    // 从状态机的状态
    enum LINE_STATUS
    {
        LINE_OK = 0,// 完整读取一行
        LINE_BAD,// 报文语法有误
        LINE_OPEN// 读取的行不完整
    };
    // 报文的解析结果
    enum HTTP_CODE
    {
        NO_REQUEST,// 请求不完整
        GET_REQUEST,// 获得了完整的http请求
        BAD_REQUEST,// http请求报文有语法错误或请求资源为目录
        NO_RESOURCE,
        FORBIDDEN_REQUEST,// 请求资源禁止访问
        FILE_REQUEST,// 请求资源可以正常访问
        INTERNAL_ERROR,// 服务器内部错误
        CLOSED_CONNECTION
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    // sockaddr：是个结构体，成员变量包括地址族、端口号、IP地址等  
    // ?????????????????????????????????????????????????????????????????
    // 初始化连接，将sockfd添加到epollfd中
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, int, string user, string passwd, string sqlname);
    void close_conn(bool real_close = true);

    void process();// proactor模式调用该函数，线程池中的线程只负责处理逻辑任务


private:
    // 初始化类中的一些变量
    void init();

    char *getline() {return m_read_buf + m_start_line;}

    LINE_STATUS parse_line();// 解析已经存在缓冲区中的请求报文（将'\r\n'变为'\0\0'）

public:
    static int m_epollfd;
    static int m_user_count;
    int m_state;// 读还是写


private:   
    int m_sockfd;
    sockaddr_in m_address;

    char m_read_buf[READ_BUFFER_SIZE];// 存储读取到的请求报文数据
    int m_read_idx;
    int m_checked_idx;
    int m_start_line;

    char m_write_buf[WRITE_BUFFER_SIZE];// 存储发出的响应报文数据
    int m_write_idx;

    CHECK_STATE m_check_state;
    METHOD m_method;

    // 解析请求报文中的相关变量
    char m_real_file[FILENAME_LEN];// 要请求的html文件的名字
    char *m_url;
    char *m_version;
    char *m_host;
    int m_content_length;
    bool m_linger;// 标识是否为长连接

    char *m_file_address;// 要读取的文件在服务器上的地址
    struct stat m_file_stat;// 描述文件相关属性的结构体
    struct iovec m_iv[2];
    int m_iv_count;//???????????????????????????????
    int cgi;

    char *m_string;// 存储请求报文的消息体
    int bytes_to_send;
    int bytes_have_send;
    char *doc_root;



    map<string, string> m_users;
    int m_SQLVerify;
    int m_TRIGMode;// 应该是标记ET/LT
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};




#endif