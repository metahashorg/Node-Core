#ifndef SocketServer_HPP
#define SocketServer_HPP

#include <io_service.hpp>

class SocketClient {
private:
    ThreadPool& TP;
    IO_SERVICE& io_service;
    std::string host;
    std::string ip;
    int port;
    int socket = 0;
    int max_conn = 0;

public:
    SocketClient(IO_SERVICE& _io_service, std::string host, int port, int max_conn);
    void make_connection();
    void get_ip();
    void send_message(uint64_t timeout_ms, std::vector<char>& data, std::function<void(SOCKET_IO_DATA&)> on_ok, std::function<void(SOCKET_IO_DATA&)> on_err);
    SOCKET_IO_DATA* new_msg();
};

#endif
