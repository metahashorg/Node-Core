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
    //    DEBUG_COUT("EPOLLOUT \t-\t" + int2bin(EPOLLOUT));
    //    DEBUG_COUT("EPOLLIN  \t-\t" + int2bin(EPOLLIN));
    //    DEBUG_COUT("EPOLLET  \t-\t" + int2bin(EPOLLET));
    //    DEBUG_COUT("EPOLLRDHUP\t-\t" + int2bin(EPOLLRDHUP));
    //    DEBUG_COUT("EPOLLHUP \t-\t" + int2bin(EPOLLHUP));
    //    DEBUG_COUT("EPOLLERR \t-\t" + int2bin(EPOLLRDHUP));
}

IO_SERVICE::~IO_SERVICE()
{
    close(epoll_io_fd);
}

void IO_SERVICE::add_sock(SOCKET_IO_DATA* data)
{
    make_socket_non_blocking(data->sock);
    io_queue.enqueue(data);
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

    bool sleep = true;

    uint64_t got_sock_size = io_queue.try_dequeue_bulk(io_sockets.begin(), io_sockets.size());
    if (got_sock_size > 0) {
        for (uint i = 0; i < got_sock_size; i++) {
            struct epoll_event event {
            };
            event.data.ptr = static_cast<void*>(dynamic_cast<SOCKET_IO_DATA*>(io_sockets[i]));
            event.events = EPOLLOUT | EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
            DEBUG_COUT(std::to_string(io_sockets[i]->sock) + "\tADD SOCKET");
            if (epoll_ctl(epoll_io_fd, EPOLL_CTL_ADD, io_sockets[i]->sock, &event) < 0) {
                perror("IO_SERVICE::io_worker epoll_ctl ADD");
            }
        }
        sleep = false;
    }

    int events_count = epoll_wait(epoll_io_fd, io_events.data(), io_events.size(), 0);
    if (events_count > 0) {
        for (int i = 0; i < events_count; i++) {
            auto* data = static_cast<SOCKET_IO_DATA*>(io_events[i].data.ptr);
            int infd = data->sock;
            //            DEBUG_COUT(std::to_string(infd) + "\t-\t" + int2bin(io_events[i].events));

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
        sleep = false;
    }

    if (sleep) {
        TP.runSheduled(1, &IO_SERVICE::io_worker, this);
    } else {
        TP.runAsync(&IO_SERVICE::io_worker, this);
    }
}

int IO_SERVICE::make_socket_non_blocking(int sfd)
{
    fcntl(sfd, F_SETFL, O_NONBLOCK);
    return 0;
}
