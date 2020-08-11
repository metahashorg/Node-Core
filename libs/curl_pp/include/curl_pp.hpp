#ifndef MHCURL_HPP
#define MHCURL_HPP

#include <string>
#include <functional>

#include <boost/asio.hpp>

namespace http::client {

void send_message_with_callback(boost::asio::io_context& ioc, const std::string& host, const std::string& port, const std::string& url, const std::string& data, std::function<void(bool, std::string)>);

}

#endif // MHCURL_HPP
