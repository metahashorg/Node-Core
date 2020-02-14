#ifndef IO_SERVICE_HPP
#define IO_SERVICE_HPP

#include <socket_io_data.hpp>
#include <sys/epoll.h>

class IO_SERVICE {
private:
    std::atomic<bool> goon = true;
    uint64_t sleep_ms = 1;
    ThreadPool& TP;

    std::vector<SOCKET_IO_DATA*> io_sockets;
    std::vector<struct epoll_event> io_events;
    int epoll_io_fd = 0;

    moodycamel::ConcurrentQueue<SOCKET_IO_DATA*> io_queue;

public:
    IO_SERVICE(ThreadPool& _TP);

    ~IO_SERVICE();

    void add_sock(SOCKET_IO_DATA* data);

    void stop();

    static  int make_socket_non_blocking(int sfd);

    ThreadPool & get_tp();

private:
    void io_worker();
};

#endif
