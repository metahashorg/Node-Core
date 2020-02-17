#include <io_service.hpp>

#include <meta_log.hpp>
#include <open_ssl_decor.h>

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

IO_SERVICE::IO_SERVICE(ThreadPool& _TP)
    : TP(_TP)
    , io_sockets(256, nullptr)
    , io_events(256)
    , epoll_io_fd(epoll_create1(0))
{
    TP.runAsync(&IO_SERVICE::io_worker, this);
}

IO_SERVICE::~IO_SERVICE()
{
    close(epoll_io_fd);
}

void IO_SERVICE::add_sock(SOCKET_IO_DATA* data)
{
    make_socket_non_blocking(data->sock);
    struct epoll_event event {
    };
    event.data.ptr = static_cast<void*>(dynamic_cast<SOCKET_IO_DATA*>(data));
    event.events = EPOLLOUT | EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
    DEBUG_COUT(std::to_string(data->sock) + "\tADD SOCKET");
    if (epoll_ctl(epoll_io_fd, EPOLL_CTL_ADD, data->sock, &event) < 0) {
        perror("IO_SERVICE::io_worker epoll_ctl ADD");
    }
}

void IO_SERVICE::stop()
{
    goon.store(false);
}

void IO_SERVICE::io_worker()
{
    if (!goon.load()) {
        return;
    }

    int events_count = epoll_wait(epoll_io_fd, io_events.data(), io_events.size(), 0);
    if (events_count > 0) {
        for (uint i = 0; i < events_count; i++) {
            auto* data = static_cast<SOCKET_IO_DATA*>(io_events[i].data.ptr);
            int infd = data->sock;

            if (io_events[i].events & EPOLLRDHUP || io_events[i].events & EPOLLHUP || io_events[i].events & EPOLLERR) {
                if (epoll_ctl(epoll_io_fd, EPOLL_CTL_DEL, infd, nullptr) < 0) {
                    perror("IO_SERVICE::io_worker epoll_ctl DEL");
                }
                TP.runAsync(&SOCKET_IO_DATA::close_connection, data);
                continue;
            }

            if (io_events[i].events & EPOLLIN && data->wait_read) {
                TP.runAsync(&SOCKET_IO_DATA::read_data, data);
                continue;
            }

            if (io_events[i].events & EPOLLOUT && data->wait_write) {
                TP.runAsync(&SOCKET_IO_DATA::write_data, data);
                continue;
            }
        }
        TP.runAsync(&IO_SERVICE::io_worker, this);
    } else {
        TP.runSheduled(1, &IO_SERVICE::io_worker, this);
    }
}

int IO_SERVICE::make_socket_non_blocking(int sfd)
{
    fcntl(sfd, F_SETFL, O_NONBLOCK);
    return 0;
}
ThreadPool& IO_SERVICE::get_tp()
{
    return TP;
}
