#ifndef SocketServer_HPP
#define SocketServer_HPP

#include <io_service.hpp>

class SocketServer {
private:
    ThreadPool& TP;
    IO_SERVICE& io_service;
    std::atomic<bool> goon;

    struct epoll_event events;

    uint64_t default_timeout_ms = 1000 * 30;

    int server_socket;
    int epoll_fd;

    std::function<SOCKET_IO_DATA*()> allocator;

    std::function<void(SOCKET_IO_DATA*)> _fn_on_read;
    std::function<void(SOCKET_IO_DATA*)> _fn_on_write;
    std::function<void(SOCKET_IO_DATA*)> _fn_on_close;

    //    int make_socket_non_blocking(int sfd);

    int create_and_bind(const char* port);

    void init_listener(const char* port);

    SOCKET_IO_DATA* new_msg(int sock);

    void listener();

public:
    SocketServer(ThreadPool& _TP, IO_SERVICE& _io_service, int _port, std::function<SOCKET_IO_DATA*()> _allocator);

    void start();

    void stop();

    void set_read_coplete_action(std::function<void(SOCKET_IO_DATA*)> _on_read);

    void set_write_coplete_action(std::function<void(SOCKET_IO_DATA*)> _on_write);

    void set_closed_action(std::function<void(SOCKET_IO_DATA*)> _on_close);
};

#endif
