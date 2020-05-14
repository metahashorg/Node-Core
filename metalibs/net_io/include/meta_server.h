#ifndef METANET_META_SERVER_H
#define METANET_META_SERVER_H

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <functional>
#include <unordered_set>

#include <open_ssl_decor.h>

namespace metahash::net_io {

uint32_t METAHASH_MAGIC_NUMBER = 0x01abcdef;

class request {
public:
    uint64_t request_id = 0;
    uint64_t request_type = 0;
    std::string_view public_key;
    std::string_view sign;
    std::string_view message;

    std::string sender_addr;
    std::string remote_address;

    int8_t parse(char*, size_t, std::unordered_set<std::string, crypto::DataHasher>& allowed_addreses);

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

class reply {
public:
    uint64_t reply_id = 0;
    std::vector<char> message;

    std::vector<boost::asio::const_buffer> make(crypto::Signer&);
private:
};

class connection : public boost::enable_shared_from_this<connection> {
public:
    explicit connection(boost::asio::io_context& io_context, std::function<void(Request&, Reply&)>& handler, crypto::Signer& signer, std::unordered_set<std::string, crypto::DataHasher>& allowed_addreses);

    boost::asio::ip::tcp::socket& get_socket();

    void start();

private:
    void handle_read(const boost::system::error_code& e, std::size_t bytes_transferred);
    void handle_write(const boost::system::error_code& e);
    void handle_write_and_close(const boost::system::error_code& e);

    boost::asio::strand<boost::asio::io_context::executor_type> strand;

    boost::asio::ip::tcp::socket socket;

    boost::array<char, 0xffff> buffer;
    request request;
    reply reply;

    std::function<void(Request&, Reply&)>& request_handler;
    crypto::Signer& signer;
    std::unordered_set<std::string, crypto::DataHasher>& allowed_addreses;
};

class meta_server {
public:
    meta_server(boost::asio::io_context& io_context,
        const std::string& address,
        const std::string& port,
        std::function<void(Request&, Reply&)> request_handler,
        crypto::Signer& signer,
        std::unordered_set<std::string, crypto::DataHasher> allowed_addreses);

private:
    void start_accept();
    void handle_accept(const boost::system::error_code& e);

    boost::asio::io_context& io_context;
    boost::asio::ip::tcp::acceptor acceptor;

    std::function<void(Request&, Reply&)> request_handler;
    crypto::Signer& signer;
    std::unordered_set<std::string, crypto::DataHasher> allowed_addreses;

    boost::shared_ptr<connection> new_connection;
};

}

#endif //METANET_META_SERVER_H
