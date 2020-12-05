#include "http_conn.h"

#include <fstream>

const char *ok_200_title = "OK";

const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";

const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file from this server";

const char *error_404_title = "Not found";
const char *error_404_form = "The reuqests file was not found on this server.\n";

const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

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

void modfd(int epollfd, int fd, int ev, int TRIGMode){
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode){
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    }
    else{
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    }

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
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
    else if (strncasecmp(text, "Connection:", 11) == 0){
        text += 11;

        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0){
            m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0){
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if(strncasecmp(text, "Host:", 5) == 0){
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else{

    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char *text){
    if (m_read_idx >= (m_content_length + m_checked_idx)){
        text[m_content_length] = '\0';// 消息的末尾置为'\0'
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}



// 通过状态机逐行解析报文
http_conn::HTTP_CODE http_conn::process_read(){
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;

    char *text = 0;// 指向当前要读取的内容在读缓冲区中的起始位置

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
            if (ret == BAD_REQUEST){
                return BAD_REQUEST;
            }
            else if (ret == GET_REQUEST){映射


                return do_request();
            }
            break;
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

// 将要访问的文件mmap映射到内存
http_conn::HTTP_CODE http_conn::do_request(){
    // 将初始化的m_real_file赋值为网站根目录   m_real_file是个char数组存取读取文件的名称
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    // m_url为请求报文中解析出的请求资源，以/开头，也就是/xxx，项目中解析后的m_url有8种情况
    const char *p = strrchr(m_url, '/');

    // 将要访问的资源的完整路径存放到m_real_file
    // 如果请求资源为/0，表示跳转注册界面
    if (*(p + 1) == '0'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "register.html");
        // m_real_file是根目录  m_url_real是具体要读取的那个文件  将两者拼接
        strcpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    // 如果请求资源为/1，表示跳转登录界面
    else if (*(p + 1) == '1'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    // 图片请求页面
    else if (*(p + 1) == '5'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    // 视频请求页面
    else if (*(p + 1) == '6'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    // 关注页面
    else if (*(p + 1) == '7'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else{
        // FILENAME_LEN的大小为200  m_url为请求行的第二部分的内容
        strncpy(m_real_file + len, m_url, FILENAME_LEN - LEN - 1);
    }

    // 通过stat函数获取要请求的文件的属性
    if (stat(m_real_file, &m_file_stat) < 0){
        return NO_RESOURCE;
    }
    // 判断文件的权限是否可读
    if (S_ISDIR(m_file_stat.st_mode & S_IROTH)){
        return FORBIDDEN_REQUEST;
    }
    // 判断文件类型，如果是目录，则返回BAD_REQUEST
    if (S_ISDIR(m_file_stat.st_mode)){
        return BAD_REQUEST;
    }

    int fd = open(m_real_file, 0_ROONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVTE, fd, 0);

    close(fd);// 映射完成之后关闭文件描述符

    return FILE_REQUEST;
}


// 真正用来向写缓冲区中写数据
bool http_conn::add_response(const char *format, ...){
    if (m_write_idx >= WRITE_BUFFER_SIZE){
        return false;
    }

    va_list arg_list;// 是一个字符指针，可以理解为指向当前参数的一个指针
    va_start(arg_list, format);// 对arg_list进行初始化

    // 将第四个参数中的内容按第三个参数的格式写入参数1中，参数而表示参数1所能接受的最大长度,返回生成的字符串的长度
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)){
        va_end(arg_list);
        return false;
    }

    m_write_idx += len;
    va_end(arg_list);

    return true;
}

// int status是状态码，如404 500等
bool http_conn::add_status_line(int status, const char *title){
    // 向写缓冲区中写数据
    return add_response("%s %d %s\r\n". "HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_len){
    // 在响应报文的报头添加报文长度、长连接字段以及空行
    return add_content_length(content_len) && add_linger() && add_blank_line();
}

bool http_conn::add_content_length(int content_len){
    return add_response("Content-Length:%d\r\n", content_len);
}

bool http_conn::add_content_type(){
    return add_response("Content-Type:%s\r\n", "text/html");
}

bool http_conn::add_blank_line(){
    return add_response("%s", "\r\n");
}

// mmap将一个文件或者其它对象映射进内存。文件被映射到多个页上，如果文件的大小不是所有页的大小之和，
// 最后一个页不被使用的空间将会清零。
// munmap执行相反的操作，删除特定地址区域的对象映射。 
void HTTTP_CONN::unmap(){
    if (m_file_address){
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

// 写响应报文
bool http_conn::process_write(HTTP_CODE ret){
    switch (ret)
    {
    // 内部错误500
    case INTERNAL_ERROR:{
        // 状态行
        add_status_line(500, error_500_title);
        // 消息报头
        add_headers(strlen(error_500_form));
        // 消息体
        if (!add_content(error_500_form)){
            return false;
        }
        break;
    }
    // 报文语法有误，404
    case BAD_REQUEST:{
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    // 资源没有访问权限，403
    case FORBIDDEN_REQUEST:{
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form)){
            return false;
        }
        break;
    }
    // 要访问的资源存在 200
    case FILE_REQUEST:{
        add_status_line(200, ok_200_title);
        // 文件内容长度不等于1
        if (m_file_stat.st_size != 0){
            add_headers(m_file_stat.st_size);

            // 指向写缓冲区
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            // 指向mmap返回的文件指针
            m_iv[1].iv_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;

            m_iv_count = 2;

            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        // 要请求的文件的长度为0
        else{
            // 返回空白html文件
            const char *ok_string = "<html><body></body></html>"
            add_headers(strlen(ok_string))
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    // 没有要请求的文件时
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

// proactor模式调用该函数，只处理逻辑任务
void http_conn::process(){
    // 请求报文已存储到缓冲区，此处需要解析请求报文
    HTTP_CODE read_ret = process_read();

    // NO_REQUEST标识请求不完整，需要继续接收请求数据
    if (read_ret == NO_REQUEST){
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }
    // 调用process_write完成报文响应
    bool write_ret = process_write(read_ret);
    if (!write_ret){
        close_conn();
    }

    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}