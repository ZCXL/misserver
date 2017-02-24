//
// Created by 朱超 on 2017/2/23.
//

#ifndef MIS_MISWORK_H
#define MIS_MISWORK_H
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <string>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include "work.h"

namespace mis {
#define FIFO_FILE "/tmp/mis.temp"
#define MAX_EPOLL_EVENT_NUM 256
#define MAX_FD_NUM 1024
#define INVALID_FD -1
#define READ_BUF_SIZE 1024
#define LISTEN_PORT 10007
#define THREAD_NUM 20
#define SOCKET_SND_BUF_SIZE (1024*1024)
#define SOCKET_RCV_BUF_SIZE (1024*1024)
#define RECEIVE_TIMEOUT 50

    class MisWork {
    public:
        MisWork();
        ~MisWork();
        int Init();
        int Run();
        int Stop();
    private:
        int _epoll_fd; //epoll文件描述符
        int _fifo_fd; //打开fifo文件
        int _fifo; //fifo创建结果
        int _socket_server_listen;
        int _epoll_ready_num; //准备就绪的fd
        int _fd_size; //当前待处理的fd
        int _stop_task; //是否停止任务
        int _fd_ary[MAX_FD_NUM]; //fd就绪数组
        epoll_event _epoll_ready_event[MAX_EPOLL_EVENT_NUM];
        std::string _str_buf;

        pthread_mutex_t _epoll_mutex;
        pthread_t *_thread;
        size_t _thread_num;
        pthread_barrier_t _barrier;
    private:
        int create_thread(size_t thread_num);
        int create_listen(int &, int);
        int set_socket(int, int);
        int add_input_fd(int);
        int recv_request(int, Work &);
        int process_request(int);
        int readn_timeout(int fd, std::string& str_buf, timeval *timeout);
        int read_data(int fd, void* buf ,size_t len, timeval *timeout);
        int svc();
        int join();
        static void *run_svc(void *arg);
    };
}
#endif //MIS_MISWORK_H
