#include <socket_client.hpp>

#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <sys/socket.h>

SocketClient::SocketClient(IO_SERVICE& _io_service, std::string host, int port, int max_conn)
    : TP(_io_service.get_tp())
    , io_service(_io_service)
    , host(host)
    , port(port)
    , max_conn(max_conn)
{
    get_ip();
    make_connection();
}

void SocketClient::get_ip()
{
    struct hostent* he;
    struct in_addr** addr_list;

    if ((he = gethostbyname(host.c_str())) == nullptr) {
        herror("gethostbyname");
        exit(0);
    }

    addr_list = (struct in_addr**)he->h_addr_list;

    for (int i = 0; addr_list[i] != nullptr; i++) {
        ip = std::string(inet_ntoa(*addr_list[i]));
    }
}

void SocketClient::make_connection()
{
    for (uint i = 0; i < max_conn; i++) {
        auto msg = new_msg();

        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        make_socket_non_blocking(sockfd);

        struct sockaddr_in serv_addr;
        memset(&serv_addr, '0', sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        serv_addr.sin_addr.s_addr = inet_addr(ip.c_str());

        if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("connect");
            exit(1);
        }

        TP.runAsync(&IO_SERVICE::add_sock, &io_service, msg);
        TP.runAsync(&SOCKET_IO_DATA::check_timeout, msg, default_timeout_ms);
    }
}

void SocketClient::send_message(
    uint64_t timeout_ms,
    std::vector<char>& data,
    std::function<void(SOCKET_IO_DATA&)> on_success,
    std::function<void(SOCKET_IO_DATA&)> on_error)
{
    int socket;
    SOCKET_IO_DATA* sd;
    if (hosts.find(host, port) != hosts.end()) {
        sd, socket = hosts.at(host, port);
    } else {
        sd, socket = new_connection(host, port);
    }

    sd->_fn_on_write = callback;
    sd->write_buff.insert(sd->write_buff.end(), data.begin(), data.end());

    io_service.write_sock(socket, sd);
}

SOCKET_IO_DATA* SocketClient::new_msg()
{
}
