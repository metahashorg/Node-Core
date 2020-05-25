#ifndef METANET_META_SERVER_H
#define METANET_META_SERVER_H

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <functional>
#include <unordered_set>

#include <open_ssl_decor.h>

#include <meta_common.h>

namespace metahash::net_io {

class Request {
public:
    uint64_t request_id = 0;
    uint64_t request_type = 0;
    std::string_view public_key;
    std::string_view sign;
    std::string_view message;

    std::string sender_mh_addr;
    std::string remote_ip_address;

    int8_t parse(char*, size_t, std::unordered_set<std::string, crypto::Hasher>& allowed_addreses);

private:
    std::vector<char> request_full;
    uint64_t offset = 0;

    uint32_t magic_number = 0;
    uint64_t public_key_size = 0;
    uint64_t sign_size = 0;
    uint64_t message_size = 0;

    bool read_varint(uint64_t& varint);
    bool fill_sw(std::string_view& sw, uint64_t sw_size);
};

class Reply {
public:
    uint64_t reply_id = 0;
    std::vector<char> message;

    boost::asio::const_buffer make(crypto::Signer&);

private:
    std::vector<char> write_buff;
};

struct Connection {
    explicit Connection(boost::asio::io_context& io_context, std::function<void(Request&, Reply&)> handler, crypto::Signer& signer, std::unordered_set<std::string, crypto::Hasher>& allowed_addreses);

    static void start(std::shared_ptr<Connection>);

    void read(std::shared_ptr<Connection>);
    void write(std::shared_ptr<Connection>);
    void write_and_close(std::shared_ptr<Connection>);
    void write_http_close(std::shared_ptr<Connection>);

    boost::asio::ip::tcp::socket socket;
    boost::array<char, 0xffff> buffer{};

    Request request;
    Reply reply;

    std::function<void(Request&, Reply&)> request_handler;
    crypto::Signer& signer;
    std::unordered_set<std::string, crypto::Hasher>& allowed_addreses;
};

class meta_server {
public:
    meta_server(boost::asio::io_context& io_context,
        const std::string& address,
        int port,
        crypto::Signer& signer,
        std::function<void(Request&, Reply&)> request_handler);

    void start();

    void update_allowed_addreses(std::unordered_set<std::string, crypto::Hasher> allowed_addreses);

private:
    void start_accept();
    void handle_accept(const boost::system::error_code& e);

    boost::asio::io_context& io_context;
    boost::asio::ip::tcp::endpoint endpoint;
    boost::asio::ip::tcp::acceptor acceptor;

    std::function<void(Request&, Reply&)> request_handler;
    crypto::Signer& signer;
    std::unordered_set<std::string, crypto::Hasher> allowed_addreses;

    std::shared_ptr<Connection> new_connection;
};

}

#endif //METANET_META_SERVER_H
