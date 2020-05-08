//
// Created by 79173 on 07.05.2020.
//

#ifndef METANET_META_HTTP_SERVER_H
#define METANET_META_HTTP_SERVER_H

#include <unordered_map>

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>

struct request {
    uint64_t part_index = 0;
    uint64_t part_expected_size = 0;
    uint32_t magic_number;
    uint64_t request_size;
    std::vector<char> request_type;
    std::vector<char> public_key;
    std::vector<char> sign;
    std::vector<char> message;

    int8_t parse(char *, size_t);

    int current_state = 0;
    std::unordered_map<std::string, std::pair<int, int>> state{
        {"magic_number_state",{0,4}},
        {"request_size_state",{0,0}},
        {"request_type_state",{0,0}},
        {"public_key_size_state",{0,0}},
        {"public_key_state",{0,0}},
        {"sign_size_state",{0,0}},
        {"sign_state",{0,0}},
        {"message_size_state",{0,0}},
        {"message_size",{0,0}},
        {"message_state",{0,0}}
    };
};

class connection
    : public boost::enable_shared_from_this<connection> {
public:
    explicit connection(boost::asio::io_context& io_context);

    boost::asio::ip::tcp::socket& get_socket();

    void start();

private:
    void handle_read(const boost::system::error_code& e,
        std::size_t bytes_transferred);

    void handle_write(const boost::system::error_code& e);

    boost::asio::strand<boost::asio::io_context::executor_type> strand;

    boost::asio::ip::tcp::socket socket;

    boost::array<char, 0xffff> buffer;

    request request;

};

class meta_http_server {
public:
    explicit meta_http_server(boost::asio::io_context& io_context, const std::string& address, const std::string& port);

private:
    void start_accept();

    void handle_accept(const boost::system::error_code& e);

    boost::asio::io_context& io_context;

    boost::asio::ip::tcp::acceptor acceptor;

    boost::shared_ptr<connection> new_connection;
};

#endif //METANET_META_HTTP_SERVER_H
