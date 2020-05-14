#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>

#include <meta_server.h>
#include <version.h>

namespace metahash::net_io {

namespace statics {
    enum parse_state {
        SUCCESS,
        INCOMPLETE,
        WRONG_MAGIC_NUMBER,
        UNKNOWN_SENDER_METAHASH_ADDRESS,
        INVALID_SIGN,
    };

    std::vector<char> version_info(const std::string& mh_addr)
    {
        static const std::string version = std::string(VESION_MAJOR) + "." + std::string(VESION_MINOR) + "." + std::string(GIT_COUNT);
        std::string resp_data = "{\"result\":{\"version\":\"" + version + "\",\"mh_addr\": \"" + mh_addr + "\"}}";
        static const std::string http_head = "HTTP/1.1 200 OK\r\n"
                                             "Version: HTTP/1.1\r\n"
                                             "Content-Type: text/json; charset=utf-8\r\n"
                                             "Content-Length: ";
        static const std::string rnrn = "\r\n\r\n";

        std::vector<char> write_buff;
        write_buff.insert(write_buff.end(), http_head.begin(), http_head.end());
        std::string data_size_str = std::to_string(resp_data.size());
        write_buff.insert(write_buff.end(), data_size_str.begin(), data_size_str.end());
        write_buff.insert(write_buff.end(), rnrn.begin(), rnrn.end());
        write_buff.insert(write_buff.end(), resp_data.begin(), resp_data.end());
    }

    std::vector<char> unkown_sender()
    {
        std::vector<char> write_buff;

        return write_buff;
    }

    std::vector<char> invalid_sign()
    {
        std::vector<char> write_buff;

        return write_buff;
    }
}

meta_server::meta_server(boost::asio::io_context& io_context,
    const std::string& address,
    const std::string& port,
    std::function<void(Request&, Reply&)> request_handler,
    crypto::Signer& signer,
    std::unordered_set<std::string, crypto::DataHasher> allowed_addreses)
    : io_context(io_context)
    , acceptor(io_context)
    , request_handler(request_handler)
    , signer(signer)
    , allowed_addreses(allowed_addreses)
    , new_connection()
{
    boost::asio::ip::tcp::resolver resolver(io_context);
    boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(address, port).begin();
    acceptor.open(endpoint.protocol());
    acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    acceptor.bind(endpoint);
    acceptor.listen();

    start_accept();
}

void meta_server::start_accept()
{
    new_connection.reset(new connection(io_context, request_handler, signer, allowed_addreses));
    acceptor.async_accept(new_connection->get_socket(), boost::bind(&meta_server::handle_accept, this, boost::asio::placeholders::error));
}

void meta_server::handle_accept(const boost::system::error_code& e)
{
    if (!e) {
        new_connection->start();
    }

    start_accept();
}

connection::connection(boost::asio::io_context& io_context, std::function<void(Request&, Reply&)>& handler, crypto::Signer& signer, std::unordered_set<std::string, crypto::DataHasher>& allowed_addreses)
    : strand(boost::asio::make_strand(io_context.get_executor()))
    , socket(strand)
    , request_handler(handler)
    , signer(signer)
    , allowed_addreses(allowed_addreses)
{
}

boost::asio::ip::tcp::socket& connection::get_socket()
{
    return socket;
}

void connection::start()
{
    socket.async_read_some(boost::asio::buffer(buffer), boost::bind(&connection::handle_read, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void connection::handle_read(const boost::system::error_code& e, std::size_t bytes_transferred)
{
    if (!e) {
        auto result = static_cast<statics::parse_state>(request.parse(buffer.data(), bytes_transferred));

        switch (result) {
        case statics::SUCCESS: {
            request.remote_address = socket.remote_endpoint().address().to_string();
            reply.reply_id = request.request_id;

            request_handler(request, reply);

            boost::asio::async_write(socket, reply.make(signer), boost::bind(&connection::handle_write, shared_from_this(), boost::asio::placeholders::error));
        } break;
        case statics::INCOMPLETE: {
            socket.async_read_some(boost::asio::buffer(buffer), boost::bind(&connection::handle_read, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
        } break;
        case statics::WRONG_MAGIC_NUMBER: {
            boost::asio::async_write(socket, boost::asio::buffer(statics::version_info(signer.get_mh_addr())), boost::bind(&connection::handle_write_and_close, shared_from_this(), boost::asio::placeholders::error));
        } break;
        case statics::UNKNOWN_SENDER_METAHASH_ADDRESS: {
            reply.message = statics::unkown_sender();
            boost::asio::async_write(socket, reply.make(signer), boost::bind(&connection::handle_write_and_close, shared_from_this(), boost::asio::placeholders::error));
        } break;
        case statics::INVALID_SIGN: {
            reply.message = statics::invalid_sign();
            boost::asio::async_write(socket, reply.make(signer), boost::bind(&connection::handle_write_and_close, shared_from_this(), boost::asio::placeholders::error));
        } break;
        default: {
            boost::asio::async_write(socket, boost::asio::buffer(statics::version_info(signer.get_mh_addr())), boost::bind(&connection::handle_write_and_close, shared_from_this(), boost::asio::placeholders::error));
        }
        }
    }
}

void connection::handle_write(const boost::system::error_code& e)
{
    if (!e) {
        start();
    }
}

void connection::handle_write_and_close(const boost::system::error_code& e)
{
    if (!e) {
        boost::system::error_code ignored_ec;
        socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
    }
}

std::vector<boost::asio::const_buffer> reply::make(crypto::Signer& signer)
{
    std::vector<char> write_buff;

    std::vector<char> public_key = signer.get_pub_key();
    std::vector<char> sign = signer.sign(message);

    write_buff.insert(write_buff.end(), reinterpret_cast<char*>(&METAHASH_MAGIC_NUMBER), (reinterpret_cast<char*>(&METAHASH_MAGIC_NUMBER) + sizeof(uint32_t)));
    crypto::append_varint(write_buff, reply_id);

    crypto::append_varint(write_buff, public_key.size());
    write_buff.insert(write_buff.end(),public_key.begin(), public_key.end());

    crypto::append_varint(write_buff, sign.size());
    write_buff.insert(write_buff.end(),sign.begin(), sign.end());

    crypto::append_varint(write_buff, message.size());
    write_buff.insert(write_buff.end(),message.begin(), message.end());
}

bool request::read_varint(uint64_t& varint)
{
    auto previous_offset = offset;
    offset += crypto::read_varint(varint, std::string_view(&request_full[offset], request_full.size() - offset));
    return offset != previous_offset;
}

bool request::fill_sw(std::string_view& sw, uint64_t sw_size)
{
    if (offset + sw_size < request_full.size()) {
        return false;
    } else {
        sw = std::string_view(&request_full[offset], sw_size);
        offset += sw_size;
        return true;
    }
}

int8_t request::parse(char* buff_data, size_t buff_size, std::unordered_set<std::string, crypto::DataHasher>& allowed_addreses)
{
    request_full.insert(request_full.end(), buff_data, buff_data + buff_size);

    if (magic_number == 0) {
        if (request_full.size() >= sizeof(uint32_t)) {
            magic_number = *(reinterpret_cast<uint32_t*>(&request_full[0]));
            if (magic_number != METAHASH_MAGIC_NUMBER) {
                return statics::WRONG_MAGIC_NUMBER;
            }
            offset = sizeof(uint32_t);
        } else {
            return statics::INCOMPLETE;
        }
    }

    if (request_id == 0 && !read_varint(request_id)) {
        return statics::INCOMPLETE;
    }

    if (request_type == 0 && !read_varint(request_type)) {
        return statics::INCOMPLETE;
    }

    if (public_key_size == 0 && !read_varint(public_key_size)) {
        return statics::INCOMPLETE;
    }

    if (public_key.empty()) {
        if (fill_sw(public_key, public_key_size)) {
            sender_addr = "0x" + crypto::bin2hex(crypto::get_address(public_key));
            if (!allowed_addreses.empty()) {
                if (allowed_addreses.find(sender_addr) == allowed_addreses.end()) {
                    return statics::UNKNOWN_SENDER_METAHASH_ADDRESS;
                }
            }
        } else {
            return statics::INCOMPLETE;
        }
    }

    if (sign_size == 0 && !read_varint(sign_size)) {
        return statics::INCOMPLETE;
    }

    if (sign.empty() && fill_sw(sign, sign_size)) {
        return statics::INCOMPLETE;
    }

    if (message_size == 0 && !read_varint(message_size)) {
        return statics::INCOMPLETE;
    }

    if (message.empty()) {
        if (fill_sw(message, message_size)) {
            if (!crypto::check_sign(message, sign, public_key)) {
                return statics::INVALID_SIGN;
            }
        } else {
            return statics::INCOMPLETE;
        }
    }

    return statics::SUCCESS;
}

}