#ifndef METANET_META_SERVER_H
#define METANET_META_SERVER_H

#include <boost/array.hpp>
#include <functional>
#include <unordered_set>

#include <meta_crypto.h>
#include <meta_pool.hpp>

namespace metahash::network {

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

struct Connection {
private:
    class Reply {
    public:
        template <typename Container>
        void make(crypto::Signer& _signer, uint64_t reply_id, const Container& data)
        {
            std::vector<char> message;
            message.insert(message.end(), data.begin(), data.end());

            serialize(_signer, reply_id, message);
        }

        template <typename Container>
        void make_http(const Container& data)
        {
            write_buff.clear();
            write_buff.insert(write_buff.end(), data.begin(), data.end());
        }

        boost::asio::const_buffer make_buff();
        bool is_complete(uint64_t);

    private:
        void serialize(crypto::Signer&, uint64_t, std::vector<char>&);

        uint64_t offset = 0;
        std::vector<char> write_buff;
    };

public:
    explicit Connection(boost::asio::io_context& io_context, std::function<std::vector<char>(Request&)> handler, crypto::Signer& signer, std::unordered_set<std::string, crypto::Hasher>& allowed_addreses);

    static void start(std::shared_ptr<Connection>);

    void read(std::shared_ptr<Connection>);
    static void write(std::shared_ptr<Connection>);
    static void write_and_close(std::shared_ptr<Connection>);

    boost::asio::io_context::strand serial_execution;
    boost::asio::ip::tcp::socket socket;
    boost::array<char, 0xffff> buffer {};

    Request request;
    Reply reply;

    std::function<std::vector<char>(Request&)> request_handler;
    crypto::Signer& signer;
    std::unordered_set<std::string, crypto::Hasher>& allowed_addreses;
};

class meta_server {
public:
    meta_server(boost::asio::io_context& io_context, const std::string& address, const int port, crypto::Signer& signer, std::function<std::vector<char>(Request&)> request_handler);

    void start();

    void update_allowed_addreses(std::unordered_set<std::string, crypto::Hasher> allowed_addreses);

private:
    void start_accept();
    void handle_accept(const boost::system::error_code& e);

    boost::asio::io_context& io_context;
    boost::asio::ip::tcp::endpoint endpoint;
    boost::asio::ip::tcp::acceptor acceptor;

    const std::string my_address;
    const int my_port;

    std::function<std::vector<char>(Request&)> request_handler;
    crypto::Signer& signer;
    std::unordered_set<std::string, crypto::Hasher> allowed_addreses;

    std::shared_ptr<Connection> new_connection;
};

}

#endif //METANET_META_SERVER_H
