#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>
#include <meta_server.h>
#include <open_ssl_decor.h>

meta_server::meta_server(boost::asio::io_context& io_context, const std::string& address, const std::string& port)
    : io_context(io_context)
    , acceptor(io_context)
    , new_connection()
    , request_handler()
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
    new_connection.reset(new connection(io_context, request_handler));
    acceptor.async_accept(new_connection->get_socket(), boost::bind(&meta_server::handle_accept, this, boost::asio::placeholders::error));
}

void meta_server::handle_accept(const boost::system::error_code& e)
{
    if (!e) {
        new_connection->start();
    }

    start_accept();
}

connection::connection(boost::asio::io_context& io_context, request_handler& handler)
    : strand(boost::asio::make_strand(io_context))
    , socket(strand)
    , request_handler(handler)
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
        int8_t result = request.parse(buffer.data(), bytes_transferred);

        switch (result) {
        case 1: {
            request.remote_address = socket.remote_endpoint().address().to_string();
            std::vector<char> reply = request_handler(request);
            boost::asio::async_write(socket, reply, boost::bind(&connection::handle_write, shared_from_this(), boost::asio::placeholders::error));
        } break;
        case 0: {
            socket.async_read_some(boost::asio::buffer(buffer), boost::bind(&connection::handle_read, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
        } break;
        default: {
            reply = reply::stock_reply(reply::bad_request);
            boost::asio::async_write(socket, version_info, boost::bind(&connection::handle_write, shared_from_this(), boost::asio::placeholders::error));
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

std::vector<boost::asio::const_buffer> reply::to_buffers()
{
    std::vector<boost::asio::const_buffer> buffers;

    return buffers;
}

bool request::read_varint(uint64_t &varint)
{
    auto previous_offset = offset;
    offset += ::read_varint(varint, std::string_view(&request_full[offset], request_full.size() - offset));
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

int8_t request::parse(char* buff_data, size_t buff_size)
{
    request_full.insert(request_full.end(), buff_data, buff_data + buff_size);

    if (magic_number == 0) {
        if (request_full.size() >= sizeof(uint32_t)) {
            magic_number = *(reinterpret_cast<uint32_t*>(&request_full[0]));
            if (magic_number != METAHASH_MAGIC_NUMBER) {
                return WRONG_MAGIC_NUMBER;
            }
            offset = sizeof(uint32_t);
            current_state = 10;
        } else {
            return 0;
        }
    }

    if (request_id == 0 && !read_varint(request_id)) {
        return 0;
    }

    if (request_type == 0 && !read_varint(request_type)) {
        return 0;
    }

    if (public_key_size == 0 && !read_varint(public_key_size)) {
        return 0;
    }

    if (public_key.empty()) {
        if (fill_sw(public_key, public_key_size)) {
            sender_addr = "0x" + bin2hex(get_address(public_key));
            if (!allowed_addreses.empty()) {
                if (allowed_addreses.find(get_address(public_key)) == allowed_addreses.end()) {
                    return UNKNOWN_SENDER_METAHASH_ADDRESS;
                }
            }
        } else {
            return 0;
        }
    }

    if (sign_size == 0 && !read_varint(sign_size)) {
        return 0;
    }

    if (sign.empty() && fill_sw(sign, sign_size)) {
        return 0;
    }

    if (message_size == 0 && !read_varint(message_size)) {
        return 0;
    }

    if (message.empty()) {
        if (fill_sw(message, message_size)) {
            if (!check_sign(message, sign, public_key)) {
                return INVALID_SIGN;
            }
        } else {
            return 0;
        }
    }

    return 1;
}
