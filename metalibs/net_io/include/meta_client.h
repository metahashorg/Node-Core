#ifndef METANET_META_CLIENT_H
#define METANET_META_CLIENT_H

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <iostream>
#include <istream>
#include <ostream>
#include <string>

#include <concurrentqueue.h>
#include <open_ssl_decor.h>

#include <meta_common.h>

namespace metahash::net_io {

struct Task {
    std::vector<char> write_buff;
    std::function<void(std::vector<char>)> callback;
};

class ClientConnection {
private:
    class Response {
    public:
        uint64_t request_id = 0;
        std::vector<char> public_key;
        std::vector<char> sign;
        std::vector<char> message;

        std::string sender_addr;

        int8_t parse(char*, size_t, const std::string& mh_endpoint_addr);

    private:
        std::vector<char> request_full;
        uint64_t offset = 0;

        uint32_t magic_number = 0;
        uint64_t public_key_size = 0;
        uint64_t sign_size = 0;
        uint64_t message_size = 0;

        bool read_varint(uint64_t& varint);
        bool fill_sw(std::vector<char>& sw, uint64_t sw_size);
    };

    const std::string mh_endpoint_addr;

    boost::asio::io_context& io_context;
    const boost::asio::ip::tcp::resolver::results_type endpoints;
    std::shared_ptr<boost::asio::ip::tcp::socket> socket;

    moodycamel::ConcurrentQueue<Task*>& tasks;

    Task* p_task = nullptr;
    Response response;

    boost::asio::deadline_timer timer;
    std::array<char, 0xffff> buffer = { {} };

    bool connected = false;

public:
    ClientConnection(boost::asio::io_context& io_context,
        boost::asio::ip::tcp::resolver::results_type& endpoints,
        moodycamel::ConcurrentQueue<Task*>& tasks, std::string mh_endpoint_addr);

    void try_connect();
    bool online();

private:
    void check_tasks();
    void read();
    void reset();
};

class meta_client {
public:
    meta_client(boost::asio::io_context& io_context, const std::string& mh_endpoint_addr, const std::string& server, const int port, const int max_connections, crypto::Signer& signer);
    ~meta_client();

    void send_message(uint64_t request_type, const std::vector<char>& message, const std::function<void(std::vector<char>)>& callback);
    std::tuple<std::string, std::string, int> get_definition();

    bool online();

private:
    std::atomic<int> request_count = 0;
    moodycamel::ConcurrentQueue<Task*> tasks;

    boost::asio::io_context& io_context;
    boost::asio::ip::tcp::resolver resolver;
    boost::asio::ip::tcp::resolver::results_type endpoints;
    std::vector<ClientConnection> sockets;

    crypto::Signer& signer;

    const std::string mh_addr;
    const std::string server;
    const int port;
};
}

#endif //METANET_META_CLIENT_H
