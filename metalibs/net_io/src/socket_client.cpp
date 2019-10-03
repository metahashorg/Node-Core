#include <socket_client.hpp>

//SocketClient::SocketClient(ThreadPool& _TP, IO_SERVICE& _io_service)
//    : TP(_TP)
//    , io_service(_io_service)
//{
//}

//void SocketClient::send_message(
//    const std::string& host,
//    int port,
//    std::vector<char>& data,
//    uint64_t timeout_ms,
//    std::function<void(SOCKET_IO_DATA&)> callback)
//{
//    int socket;
//    SOCKET_IO_DATA* sd;
//    if (hosts.find(host, port) != hosts.end()) {
//        sd, socket = hosts.at(host, port);
//    } else {
//        sd, socket = new_connection(host, port);
//    }

//    sd->_fn_on_write = callback;
//    sd->write_buff.insert(sd->write_buff.end(), data.begin(), data.end());

//    io_service.write_sock(socket, sd);
//}
