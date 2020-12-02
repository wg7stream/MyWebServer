#include "http_conn.h"


// 初始化静态变量
int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;


void http_conn::init()
{
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    // Memset 用来对一段内存空间全部设置为某个字符  将m_read_buf中设置为'\0' 
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

int setnoblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool one_shot, int TRIGMode){
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode){
        // LT
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }
    else{
        // 设置为LT模式，不需要写EPOLLLT
        // EPOLLRDHUP 事件，代表对端断开连接
        // 对端连接断开触发的 epoll 事件会包含 EPOLLIN | EPOLLRDHUP
        event.events = EPOLLIN | EPOLLRDHUP;
    }

    if (one_shot){
        event.events |= EPOLLONESHOT;
    }
    // 注册
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

    setnoblocking(fd);
}


// ?????????????????????????????????????????????????????????????????????????????????????????????????????
// root参数应该是网站根目录
void http_conn::init(int sockfd, const sockaddr_in &addr, char* root, int SQLVerify, int TRIGMode,
                     int close_log, string user, string passwd, string sqlname)//TRIGMode应该是标记ET/LT
{
    m_sockfd = sockfd;
    m_address = addr;

    // 接收到新的连接，需要将该sockfd添加到epollfd中
    addfd(m_epollfd, sockfd, true, m_TRIGMode);
    m_user_count ++;

    doc_root = root;
    m_SQLVerify = SQLVerify;
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    
    init();
}

void http_conn::init(){
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;// 读
    timer_flag = 0;
    improv = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
}

// 从内核事件表中删除文件描述符
void removefd(int epllfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void http_conn::close_conn(bool real_close){
    if (real_close && (m_sockfd != -1)){
        printf("close current sockfd %d\n", m_sockfd);
        removefd(m_epollfd, m_sockfd);// 从EPOLLFD中删除
        m_sockfd = -1;
        m_user_count --;// 连接数量减1
    }
}

// 从缓冲区中解析一行的内容,如果存在完整的一行，将行尾的'\r\n'变为'\0\0'
// 此处的解析只是将原缓冲区中的'\r\n'变为'\0\0'
http_conn::LINE_STATUS http_conn::parse_line(){
    char temp;

    for (; m_checked_idx < m_read_idx; ++ m_checked_idx){
        // 当前要分析的字符
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r'){
            if ((m_checked_idx + 1) == m_read_idx){
                return LINE_OPEN;// 读取的行不完整
            }
            else if (m_read_buf[m_checked_idx + 1] == '\n'){
                m_read_buf[m_checked_idx ++] = '\0';
                m_read_buf[m_checked_idx ++] = '\0';
                return LINE_OK;// 完整的读取到了一行
            }
            return LINE_BAD;// 语法错误
        }
        else if (temp == '\n'){
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r'){
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx ++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    } 
    return LINE_OPEN;
}

// 传入该行在缓冲区的起始位置，解析请求行
http_conn::HTTP_CODE http_conn::parse_request_line(char *text){
    // 请求行中的格式：方法、url、版本号
    m_url = strpbrk(text, " \t");
    if (!m_url) return BAD_REQUEST;
    *m_url ++ = '\0';// 将该字符置为'\0',可以将它前面的字符串取出，因为字符串以'\0'结尾

    char *method = text;
    if (strcasecmp(method, "GET") == 0){
        m_method = GET;
    }
    else if (strcasecmp(method, "POST") == 0){
        m_method = POST;
    }
    else{
        return BAD_REQUESTS;
    }// 解析完请求行的第一部分

    m_url += strspn(m_url, " \t");// 得到url

    m_version = strpbrk(m_url, " \t");
    if (!m_version) return BAD_REQUEST;
    *m_version ++ = '\0';
    m_version += strspn(m_version, " \t");

    // 判断版本
    if (strcasecmp(m_version, "HTTP/1.1") != 0) return BAD_REQUEST;
    // http
    if (strncasecmp(m_url, "http://", 7) == 0){
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    // https
    if (strncasecmp(m_url, "https://", 8) == 0){
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if (!m_url || m_url[0] != '/'){
        return BAD_REQUEST;
    }

    // 当m_url为'/'时，将m_url置为"judge.html"
    if (strlen(m_url) == 1){
        strcat(m_url, "judge.html");
    }

    // 改变状态
    m_check_state = CHECK_STATE_HEADERS;
    return NO_REQUEST;
}

// 解析请求头，传入的参数是要读取的内容在缓冲区的起始位置
http_conn::HTTP_CODE http_conn::parse_headers(char *text){
    if (text[0] == '\0'){
        // 判断m_content_length是否为0，若不为0，是post请求，若为0，是get请求
        if (m_content_length != 0){
            m_check_state = CHECK_STATE_CONTENT;// 改变状态，去解析消息体
            return NO_REQUEST;
        }
        return GET_REQUEST;// 获得了完整的http请求
    }
}


// 通过状态机逐行解析报文
http_conn::HTTP_CODE http_conn::process_read(){
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;

    char *text = 0;

    while ((line_status = parse_line()) == LINE_OK
            || (m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK)){
        
        text = get_line();// 获取要读取的内容在缓冲区的起始位置，是一个指针
        m_start_line = m_checked_idx;// 在buf数组中的下标，是个int

        switch (m_check_state){
        // 解析请求行
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        // 解析请求头
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
        }
        // 解析消息体
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
        }
        }
    }

    return NO_REQUEST;
}

// proactor模式调用该函数，只处理逻辑任务
void http_conn::process(){
    // 请求报文已存储到缓冲区，此处需要解析请求报文
    HTTP_CODE read_ret = process_read();


}