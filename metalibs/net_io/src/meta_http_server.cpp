//
// Created by 79173 on 07.05.2020.
//

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>
#include <meta_http_server.h>

meta_http_server::meta_http_server(boost::asio::io_context& io_context, const std::string& address, const std::string& port)
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

void meta_http_server::start_accept()
{
    new_connection.reset(new connection(io_context, request_handler));
    acceptor.async_accept(new_connection->get_socket(),
        boost::bind(&meta_http_server::handle_accept, this, boost::asio::placeholders::error));
}

void meta_http_server::handle_accept(const boost::system::error_code& e)
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
    socket.async_read_some(boost::asio::buffer(buffer),
        boost::bind(&connection::handle_read, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void connection::handle_read(const boost::system::error_code& e,
    std::size_t bytes_transferred)
{
    if (!e) {
        int8_t result = request.parse(buffer.data(), bytes_transferred);

        switch (result) {
        case 1: {
            request_handler.handle_request(request, reply);
            boost::asio::async_write(socket, reply.to_buffers(),
                boost::bind(&connection::handle_write, shared_from_this(), boost::asio::placeholders::error));
        } break;
        case 0: {
            socket.async_read_some(boost::asio::buffer(buffer),
                boost::bind(&connection::handle_read, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
        } break;
        case -1: {
            reply = reply::stock_reply(reply::bad_request);
            boost::asio::async_write(socket, reply.to_buffers(),
                boost::bind(&connection::handle_write, shared_from_this(), boost::asio::placeholders::error));
        } break;
        default: {
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
    buffers.push_back(status_strings::to_buffer(status));
    for (auto& header : headers) {
        buffers.push_back(boost::asio::buffer(header.name));
        buffers.push_back(boost::asio::buffer(misc_strings::name_value_separator));
        buffers.push_back(boost::asio::buffer(header.value));
        buffers.push_back(boost::asio::buffer(misc_strings::crlf));
    }
    buffers.push_back(boost::asio::buffer(misc_strings::crlf));
    buffers.push_back(boost::asio::buffer(content));
    return buffers;
}

int8_t request::parse(char* buff_data, size_t buff_size)
{

    switch (current_state) {
    case state["magic_number_state"].first:
        ls;
    default:
        return false;
    }
}
