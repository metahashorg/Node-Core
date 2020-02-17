#include <socket_server.hpp>

#include <netdb.h>
#include <unistd.h>

#include <meta_log.hpp>

int SocketServer::create_and_bind(const char* port)
{
    struct addrinfo hints {
    };
    struct addrinfo *result, *rp;
    int s, sfd = 0;

    hints.ai_family = AF_UNSPEC; /* Return IPv4 and IPv6 choices */
    hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
    hints.ai_flags = AI_PASSIVE; /* All interfaces */

    s = getaddrinfo(nullptr, port, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }

    for (rp = result; rp != nullptr; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) {
            continue;
        }

        s = bind(sfd, rp->ai_addr, rp->ai_addrlen);
        if (s == 0) {
            /* We managed to bind successfully! */
            break;
        }

        close(sfd);
    }

    if (rp == nullptr) {
        fprintf(stderr, "Could not bind\n");
        return -1;
    }

    freeaddrinfo(result);

    return sfd;
}

void SocketServer::init_listener(const char* port)
{
    do {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        server_socket = create_and_bind(port);
    } while (server_socket == -1);

    if (io_service.make_socket_non_blocking(server_socket) == -1) {
        abort();
    }

    if (listen(server_socket, SOMAXCONN) == -1) {
        perror("listen");
        abort();
    }

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create");
        abort();
    }

    struct epoll_event event {
    };

    event.data.fd = server_socket;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event) == -1) {
        perror("epoll_ctl");
        abort();
    }
}

SOCKET_IO_DATA* SocketServer::new_msg(int sock)
{
    SOCKET_IO_DATA* msg = allocator();
    msg->sock = sock;
    msg->p_TP = &TP;
    msg->_fn_on_read = _fn_on_read;
    msg->_fn_on_write = _fn_on_write;
    msg->_fn_on_close = _fn_on_close;

    TP.runAsync(&IO_SERVICE::add_sock, &io_service, msg);
    TP.runAsync(&SOCKET_IO_DATA::check_timeout, msg, default_timeout_ms);

    return msg;
}

void SocketServer::listener()
{
    if (!goon.load()) {
        return;
    }

    int events_count = epoll_wait(epoll_fd, &events, 1, 0);
    if (events_count) {
        while (true) {
            int infd = accept(server_socket, nullptr, nullptr);
            if (infd < 0) {
                if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                    break;
                }
                perror("accept");
                break;
            }

            DEBUG_COUT(std::to_string(infd) + "\t-\tACCEPT CONNECTION");

            TP.runAsync(&SocketServer::new_msg, this, infd);
        }
    }

    if (events_count) {
        TP.runAsync(&SocketServer::listener, this);
    } else {
        TP.runSheduled(1, &SocketServer::listener, this);
    }
}

SocketServer::SocketServer(IO_SERVICE& _io_service, int _port, std::function<SOCKET_IO_DATA*()> _allocator)
    : TP(_io_service.get_tp())
    , io_service(_io_service)
    , goon(true)
    , allocator(std::move(_allocator))
{
    std::string port = std::to_string(_port);
    init_listener(port.c_str());
}

void SocketServer::start()
{
    TP.runAsync(&SocketServer::listener, this);
}

void SocketServer::stop()
{
    goon.store(false);
}

void SocketServer::set_read_coplete_action(std::function<void(SOCKET_IO_DATA*)> _on_read)
{
    _fn_on_read = std::move(_on_read);
}

void SocketServer::set_write_coplete_action(std::function<void(SOCKET_IO_DATA*)> _on_write)
{
    _fn_on_write = std::move(_on_write);
}

void SocketServer::set_closed_action(std::function<void(SOCKET_IO_DATA*)> _on_close)
{
    _fn_on_close = std::move(_on_close);
}
