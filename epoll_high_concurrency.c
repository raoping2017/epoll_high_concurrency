#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/resource.h>

#define MAXBUF 1024
#define MAXEPOLLSIZE 20000

#define EPOLL_HELLO "epoll hello!"

/*
setnonblocking – set  nonblocking mode
*/

/* 定义常量 */
#define HTTP_DEF_PORT        80     /* 连接的缺省端口 */
#define HTTP_BUF_SIZE      1024     /* 缓冲区的大小 */
#define HTTP_FILENAME_LEN   256     /* 文件名长度 */

/* 定义文件类型对应的 Content-Type */
struct doc_type
{
    char *suffix; /* 文件后缀 */
    char *type;   /* Content-Type */
};

struct doc_type file_type[] =
{
    {"html",    "text/html"  },
    {"gif",     "image/gif"  },
    {"jpeg",    "image/jpeg" },
    { NULL,      NULL        }
};

//Connection: Keep-Alive

char *http_res_hdr_tmpl = "HTTP/1.1 200 OK\r\nServer: Du's Server <0.1>\r\n"
    "Accept-Ranges: bytes\r\nContent-Length: %d\r\nConnection: Close\r\n"
    "Content-Type: %s\r\n\r\n";


/* 根据文件后缀查找对应的 Content-Type */
char *http_get_type_by_suffix(const char *suffix)
{
    struct doc_type *type;

    for (type = file_type; type->suffix; type++)
    {
        if (strcmp(type->suffix, suffix) == 0)
            return type->type;
    }

    return NULL;
}

/* 解析请求行 */
void http_parse_request_cmd(char *buf, int buflen, char *file_name, char *suffix)
{
    int length = 0;
    char *begin, *end, *bias;

    /* 查找 URL 的开始位置 */
    begin = strchr(buf, ' ');
    begin += 1;

    /* 查找 URL 的结束位置 */
    end = strchr(begin, ' ');
    *end = 0;

    bias = strrchr(begin, '/');
    length = end - bias;

    /* 找到文件名的开始位置 */
    if ((*bias == '/') || (*bias == '\\'))
    {
        bias++;
        length--;
    }

    /* 得到文件名 */
    if (length > 0)
    {
        memcpy(file_name, bias, length);
        file_name[length] = 0;

        begin = strchr(file_name, '.');
        if (begin)
            strcpy(suffix, begin + 1);
    }
}


/* 向客户端发送 HTTP 响应 */
int http_send_response(int soc)
{
    int read_len, file_len, hdr_len, send_len;
    char *type;
    char read_buf[HTTP_BUF_SIZE];
    char http_header[HTTP_BUF_SIZE];
    char file_name[HTTP_FILENAME_LEN] = "index.html", suffix[16] = "html";
    FILE *res_file;

    /* 得到文件名和后缀，解析 http 功能去掉，本处功能与高并发性能关系不大，这里仅返回固定 response */
    /*http_parse_request_cmd(buf, buf_len, file_name, suffix);

    res_file = fopen(file_name, "rb+");
    if (res_file == NULL)
    {
        printf("[Web] The file [%s] is not existed\n", file_name);
        return 0;
    }

    fseek(res_file, 0, SEEK_END);
    file_len = ftell(res_file);
    fseek(res_file, 0, SEEK_SET);

    type = http_get_type_by_suffix(suffix);

    if (type == NULL)
    {
        printf("[Web] There is not the related content type\n");
        return 0;
    }*/

    /* 构造 HTTP 首部，并发送 */
    hdr_len = sprintf(http_header, http_res_hdr_tmpl, strlen(EPOLL_HELLO), "text/html");
    send_len = send(soc, http_header, hdr_len, 0);
    if (send_len < 0)
    {
        //fclose(res_file);
        printf("Send mes Error!\n");
        return 0;
    }

    //do /* 发送固定字符串，非从文件 */
    {
        //read_len = fread(read_buf, sizeof(char), HTTP_BUF_SIZE, res_file);
        read_len = strlen(EPOLL_HELLO);
        memset(read_buf, 0, HTTP_BUF_SIZE);
        memcpy(read_buf, EPOLL_HELLO, read_len);

        if (read_len > 0)
        {
            send_len = send(soc, read_buf, read_len, 0);
            //file_len -= read_len;
        }
    } //while ((read_len > 0) && (file_len > 0));

    //fclose(res_file);

    return 1;
}

int setnonblocking(int sockfd)
{
    if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK) == -1)
    {
        return -1;
    }
    return 0;
}

/* 读取 http 请求，仅读出未正常解析 */
int read_http(int new_fd)
{
    char buf[MAXBUF + 1];
    int len;
    int result = 0;

    bzero(buf, MAXBUF + 1);
    len = recv(new_fd, buf, MAXBUF, 0);
    if (len < 0)
    {
        printf("recv mes failed! mistake code is%d,mistake info is '%s'\n", errno, strerror(errno));
        return -1;
    }

    return len;
}

int main( int argc, char* argv[])
{
    int listener, new_fd, kdpfd, nfds, n, ret, curfds;
    socklen_t len;
    struct sockaddr_in my_addr,their_addr;
    unsigned int myport, lisnum;
    struct epoll_event ev;
    struct epoll_event events[MAXEPOLLSIZE];
    struct rlimit rt;
    long http_count = 0;
    long fd_count = 0;
    int http_count_all = 0;
    int socket_count = 0;

    myport = 5024;
    lisnum = 2;
    /* 设置最大连接数 */
    rt.rlim_max = rt.rlim_cur = MAXEPOLLSIZE;
    if( setrlimit( RLIMIT_NOFILE, &rt) == -1 )
    {
        perror("setrlimit");
        exit(1);
    }

    /* 创建监听 socket */
    if( (listener = socket( PF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(1);
    }


    int reuse = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    setnonblocking(listener);
    my_addr.sin_family = PF_INET;
    my_addr.sin_port = htons(myport);
    my_addr.sin_addr.s_addr = INADDR_ANY;
    bzero( &(my_addr.sin_zero), 8);

    if ( bind( listener, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) == -1 )
    {
        perror("bind");
        exit(1);
    }


    if (listen(listener, lisnum) == -1)
    {
        perror("listen");
        exit(1);
    }

    /* 创建 epoll 对象 */
    kdpfd = epoll_create( MAXEPOLLSIZE );
    len = sizeof( struct sockaddr_in );
    ev.events = EPOLLIN ;//| EPOLLET;
    ev.data.fd = listener;

    if( epoll_ctl( kdpfd, EPOLL_CTL_ADD, listener, &ev) < 0 )  // rege
    {
        fprintf( stderr, "epoll set insertion error: fd=%d\n", listener );
        return -1;
    }
    curfds = 1;

    pid_t pid = fork();

    if (pid==0)
    {
        while(1)
        {
            /* 等待事件 */
            nfds = epoll_wait(kdpfd, events, MAXEPOLLSIZE, -1);
            if( nfds == -1 )
            {
                perror("epoll_wait");
                break;
            }

            for (n = 0; n < nfds; ++n)
            {
                if(events[n].data.fd == listener)
                {
                    while( (new_fd = accept( listener, (struct sockaddr*)&their_addr, &len )) < 0 )
                    {
                        perror("accept");
                        continue;
                    }

                    fd_count = fd_count +1;

                    setnonblocking(new_fd);
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = new_fd;
                    if( epoll_ctl( kdpfd, EPOLL_CTL_ADD, new_fd, &ev) < 0 )
                    {
                        fprintf(stderr, "add socket '%d' to  epoll failed! %s\n",
                                        new_fd, strerror(errno));
                        return -1;
                    }
                    curfds ++;
                }
                else if(events[n].events&EPOLLIN)
                {
                    new_fd = events[n].data.fd;
                    ret = read_http(new_fd);
                    http_count = http_count +1;
                    if (ret < 1 && errno != 11)
                    {
                        epoll_ctl(kdpfd, EPOLL_CTL_DEL, new_fd, &ev);
                        curfds--;
                        close(new_fd);
                    }
                    else
                    {
                        ev.data.fd = new_fd;
                            ev.events=EPOLLOUT|EPOLLET;
                            epoll_ctl(kdpfd,EPOLL_CTL_MOD,new_fd,&ev);//修改标识符，等待下一个循环时发送数据，异步处理的精髓
                    }
                }
                else if(events[n].events&EPOLLOUT)
                {
                    int result;
                    new_fd = events[n].data.fd;
                    result = http_send_response(new_fd);
                    epoll_ctl(kdpfd, EPOLL_CTL_DEL, new_fd, &ev);
                    curfds--;
                    close(new_fd);
                    http_count_all++;
                }
            }
        }
    }
    else
    {
        while(1)
        {
            sleep(1);
        }
    }

    close( listener );
    return 0;
}

