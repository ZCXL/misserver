//
// Created by 朱超 on 2017/2/23.
//
#include "miswork.h"
#include "work.h"
namespace mis{
    MisWork::MisWork() {
        _epoll_fd = INVALID_FD;
        _fifo_fd = INVALID_FD;
        _socket_server_listen = INVALID_FD;
        _stop_task = 0;
        _epoll_ready_num = 0;
        _fd_size = 0;
        pthread_mutex_init(&_epoll_mutex, NULL);
    }
    MisWork::~MisWork() {
        ::close(_epoll_fd);
        ::close(_fifo_fd);
        ::close(_socket_server_listen);
        pthread_mutex_destroy(&_epoll_mutex);
    }
    int MisWork::Init() {
        int options = 0;
        if (create_thread(THREAD_NUM) < 0) {
            fprintf(stderr, "create threads error!!\n");
            return -1;
        }
        /**
         * 创建fifo文件并以读写形式打开
         */
        unlink(FIFO_FILE);//如果FIFO存在，就先删除
        if ((_fifo = mkfifo(FIFO_FILE, O_RDWR)) < 0) { //产生一个有名管道
            fprintf(stderr, "mkfifo error:%s\n", strerror(errno));
            return -1;
        }
        if (chmod(FIFO_FILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) < 0) { //修改fifo文件权限
            fprintf(stderr, "chmod error:%s\n", strerror(errno));
            return -1;
        }
        if ((_fifo_fd = open(FIFO_FILE, O_RDWR)) < 0) { //读写打开有名管道
            fprintf(stderr, "open error:%s\n", strerror(errno));
            return -1;
        }

        /*设置非阻塞*/
        if ((options = fcntl(_fifo_fd, F_GETFL)) == -1){
            return -1;
        }
        if (fcntl(_fifo_fd, F_SETFL, options | O_NONBLOCK) == -1){
            return -1;
        }

        if (create_listen(_socket_server_listen, LISTEN_PORT)) {
            fprintf(stderr, "create network listen error!!\n");
            return -1;
        }

        /**
         * 创建epoll文件描述符
         */
        if ((_epoll_fd = epoll_create(MAX_FD_NUM)) == -1){
            fprintf(stderr, "error: epoll create fail!!\n");
            return -1;
        }

        /*把文件设置为监听状态*/
        if (add_input_fd(_fifo_fd) < 0) {
            fprintf(stderr, "error: add fd error!!\n");
            return -1;
        }
        if (add_input_fd(_socket_server_listen) < 0) {
            fprintf(stderr, "error: add socket error!!\n");
            return -1;
        }

        return 0;
    }

    /**
     * @function 创建处理线程
     * @param thread_num
     * @return
     */
    int MisWork::create_thread(size_t thread_num) {
        int ret = -1;
        pthread_attr_t attr;
        pthread_attr_init(&attr);

        do {
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

            if (thread_num == 0 ||
                (_thread = (pthread_t*)malloc(thread_num * sizeof(pthread_t))) == NULL){
                break;
            }

            pthread_barrier_init(&_barrier, NULL, thread_num + 1);
            size_t i = 0;
            for (i = 0; i < thread_num; i++){
                if (pthread_create(_thread + i, &attr, run_svc, this)){
                    break;
                }
            }

            if ((_thread_num = i) != thread_num){
                break;
            }

            ret = 0;
        } while (false);

        pthread_attr_destroy(&attr);
        return ret;
    }
    /**
     * @function 创建网络监听端口
     * @param socket_fd
     * @param port
     * @return
     */
    int MisWork::create_listen(int &socket_fd, int port) {
        sockaddr_in addr;
        memset(&addr, 0, sizeof addr);
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if ((socket_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1){
            return -1;
        }
        if (set_socket(socket_fd, O_NONBLOCK)){
            return -1;
        }
        if (bind(socket_fd, (const sockaddr*)&addr, sizeof addr)){
            return -1;
        }
        if (listen(socket_fd, MAX_FD_NUM)){
            return -1;
        }

        return 0;
    }

    /**
     * @function 对网络socket设置参数
     * @param fd
     * @param flag
     * @return
     */
    int MisWork::set_socket(int fd, int flag) {
        int options = SOCKET_SND_BUF_SIZE;
        setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &options, sizeof(int));
        options = SOCKET_RCV_BUF_SIZE;
        setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &options, sizeof(int));
        options = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &options, sizeof(int));
        options = fcntl(fd, F_GETFL);
        fcntl(fd, F_SETFL, options | flag);
        int on = 1;
        int ret = -1;
        ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on));
        return ret;
    }

    /**
     * @function 将fd放入epoll中监听
     * @param fd
     * @return
     */
    int MisWork::add_input_fd(int fd) {
        epoll_event event;
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = fd;
        int ret = epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &event);
        if (ret < 0) {
            fprintf(stderr, "add fd error:%s\n", strerror(errno));
        }

        return ret;
    }

    /**
     * @function 启动服务
     * @return
     */
    int MisWork::svc() {
        int fd = 0;
        int new_fd = 0;
        while(!_stop_task) {
            pthread_mutex_lock(&_epoll_mutex);

            if (_stop_task) {
                pthread_mutex_unlock(&_epoll_mutex);
                break;
            }
            //从就绪数组中获取fd
            if (_fd_size > 0) {
                fd = _fd_ary[--_fd_size];
                pthread_mutex_unlock(&_epoll_mutex);

                set_socket(fd, O_NONBLOCK);
                process_request(fd);
                continue;
            }

            //通过epoll获取就绪fd
            if (_fd_size <= 0) {
                _epoll_ready_num = epoll_wait(_epoll_fd,
                                                    _epoll_ready_event,
                                                    MAX_EPOLL_EVENT_NUM,
                                                    -1);
            }
            if (_epoll_ready_num-- < 0) {
                pthread_mutex_unlock(&_epoll_mutex);
                if (errno == EINTR) {
                    continue;
                } else {
                    break;
                }
            }

            fd = _epoll_ready_event[_epoll_ready_num].data.fd;
            //接受网络请求
            if (fd == _socket_server_listen) {
                while ((new_fd = accept(fd, NULL, NULL)) >= 0){
                    if (new_fd >= 65535){
                        close(new_fd);
                    }else{
                        _fd_ary[_fd_size++] = new_fd;
                    }
                }
                int new_pro_fd = -1;
                if (_fd_size > 0){
                    new_pro_fd = _fd_ary[--_fd_size];
                }

                pthread_mutex_unlock(&_epoll_mutex);
                if (new_pro_fd > -1){
                    set_socket(new_pro_fd, O_NONBLOCK);
                    process_request(new_pro_fd);
                }
                continue;
            } else if (fd == _fifo_fd) { //处理本地请求
                process_request(fd);
                pthread_mutex_unlock(&_epoll_mutex);
            } else {
                pthread_mutex_unlock(&_epoll_mutex);
                continue;
            }
        }
        return 0;
    }

    /**
     * @function 数据读取，设置延迟时间
     * @param fd
     * @param str_buf
     * @param timeout
     * @return
     */
    int MisWork::readn_timeout(int fd, std::string &str_buf, timeval *timeout) {
        char buf[READ_BUF_SIZE] = {0};
        int n = 0;
        int len = 0;

        str_buf = "";
        while(1) {
            if((n = read_data(fd, buf, READ_BUF_SIZE, timeout)) <= 0) {
                break;
            }
            buf[n] = '\0';
            str_buf += buf;
            len += n;
        }

	//处理本地调用的换行符号问题
	str_buf.replace(str_buf.find("\n"), 1, "");

        return len;
    }

    /**
     * @function 通过poll机制获取数据
     * @param fd
     * @param buf
     * @param len
     * @param timeout
     * @return
     */
    int MisWork::read_data(int fd, void *buf, size_t len, timeval *timeout) {
        pollfd read_fd;
        read_fd.fd = fd;
        read_fd.events = POLLIN;
        int poll_ret = poll(&read_fd, 1, timeout->tv_sec * 1000 + timeout->tv_usec / 1000);
        if (poll_ret <= 0 || !(read_fd.revents & POLLIN)) {
            return -1;
        }
        return read(fd, buf, len);
    }

    /**
     * @function 接受网络数据或本地数据
     * @param fd
     * @return
     */
    int MisWork::recv_request(int fd, Work &work) {
        timeval timeout = { 0, RECEIVE_TIMEOUT * 1000 };
        int len = 0;
        if ((len = readn_timeout(fd, work.getBuf(), &timeout)) <= 0) {
            fprintf(stdout, "recv null!!\n");
        }
        return len;
    }

    /**
     * @function 处理请求
     * @param fd
     * @return
     */
    int MisWork::process_request(int fd) {
        Work work;
        if (recv_request(fd, work) < 0) {
            fprintf(stderr, "recv null!!");
            return 0;
        }
        printf("recv: %s\n", work.getBufPtr());
        if (strcmp(work.getBufPtr(), "quit\n") == 0) {
            Stop();
        }
        return 0;
    }

    /**
     * @function 结束所有的进程
     * @return
     */
    int MisWork::join() {
        if (_thread) {
            for (size_t i = 0; i < _thread_num; i++) {
                pthread_kill(_thread[i], SIGTERM);
                pthread_join(_thread[i], NULL);
            }
            free(_thread);
            _thread = NULL;
            pthread_barrier_destroy(&_barrier);
        }
        return 0;
    }

    /**
     * @function 同步所有进程
     * @return
     */
    int MisWork::Run() {
        pthread_barrier_wait(&_barrier);
        fprintf(stdout, "server is running!!\n");
        return 0;
    }

    /**
     * @functon 停止服务
     * @return
     */
    int MisWork::Stop() {
        join();
        fprintf(stdout, "server is stoped!!\n");
        return 0;
    }

    /**
     * @function 线程处理入口函数
     * @param arg
     * @return
     */
    void *MisWork::run_svc(void *arg) {
        MisWork *mis = (MisWork *)arg;
        pthread_barrier_wait(&mis->_barrier);
        mis->svc();
        return NULL;
    }
}
