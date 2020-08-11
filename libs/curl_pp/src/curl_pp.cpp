#include <curl_pp.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <utility>
#include <boost/beast/version.hpp>

#include <memory>

#include <iostream>

namespace http::client {

struct session {
    boost::asio::ip::tcp::resolver resolver;
    boost::beast::tcp_stream stream;
    boost::beast::flat_buffer buffer;
    boost::beast::http::request<boost::beast::http::string_body> req;
    boost::beast::http::response<boost::beast::http::string_body> res;

    std::function<void(bool, std::string)> func;

    explicit session(boost::asio::io_context& ioc)
        : resolver(boost::asio::make_strand(ioc))
        , stream(boost::asio::make_strand(ioc))
    {
    }
};

void read(std::shared_ptr<session> session)
{
    boost::beast::http::async_read(session->stream, session->buffer, session->res, [session](boost::beast::error_code err, std::size_t) {
        if (err) {
            return session->func(false, "read");
        }

        session->func(true, session->res.body());

        session->stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, err);
    });
}

void write(std::shared_ptr<session> session)
{
    session->stream.expires_after(std::chrono::seconds(300));

    boost::beast::http::async_write(session->stream, session->req, [session](boost::beast::error_code err, std::size_t) {
        if (err) {
            std::cout << err.message() << std::endl;
            return session->func(false, "write");
        }

        read(session);
    });
}

void connect(std::shared_ptr<session> session, const boost::asio::ip::tcp::resolver::results_type& results)
{
    session->stream.expires_after(std::chrono::seconds(300));

    session->stream.async_connect(results, [session](boost::beast::error_code err, const boost::asio::ip::tcp::resolver::results_type::endpoint_type&) {
        if (err) {
            return session->func(false, "connect");
        }

        write(session);
    });
}

void resolve(std::shared_ptr<session> session, const std::string& host, const std::string& port)
{
    session->resolver.async_resolve(host, port, [session](boost::beast::error_code err, const boost::asio::ip::tcp::resolver::results_type& results) {
        if (err) {
            return session->func(false, "resolve");
        }

        // Set a timeout on the operation

        connect(session, results);
    });
}

void run(std::shared_ptr<session> session, const std::string& host, const std::string& port, const std::string& url, const std::string& data, std::function<void(bool, std::string)> func)
{
    session->func = std::move(func);

    session->req.method(boost::beast::http::verb::post);
    session->req.set(boost::beast::http::field::content_type, "text/plain");
    session->req.body() = data;
    session->req.target(url);
    session->req.set(boost::beast::http::field::host, host);
    session->req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    session->req.prepare_payload();

    resolve(session, host, port);
}

void send_message_with_callback(boost::asio::io_context& io_context, const std::string& host, const std::string& port, const std::string& url, const std::string& data, std::function<void(bool , std::string)> func)
{
    auto sssn = std::make_shared<session>(io_context);
    run(sssn, host, port, url, data, func);
}

}
