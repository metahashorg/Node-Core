//#include <meta_log.hpp>
#include <meta_server.h>

#include <boost/bind/bind.hpp>

namespace metahash::network {

meta_server::meta_server(boost::asio::io_context& io_context, const std::string& address, const int port, crypto::Signer& signer, std::function<std::vector<char>(Request&)> request_handler)
    : io_context(io_context)
    , acceptor(io_context)
    , my_address(address)
    , my_port(port)
    , request_handler(std::move(request_handler))
    , signer(signer)
    , new_connection()
{
    //start();
}

void meta_server::start()
{
    bool success = false;

    try {
        acceptor = boost::asio::ip::tcp::acceptor(io_context);
        endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), static_cast<unsigned short>(my_port));
        acceptor.open(endpoint.protocol());
        acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor.set_option(boost::asio::ip::tcp::acceptor::enable_connection_aborted(true));
        acceptor.bind(endpoint);
        acceptor.listen();

        success = true;
    } catch (std::exception& e) {
        //DEBUG_COUT(e.what());
        std::this_thread::sleep_for(std::chrono::seconds(1));
        start();
    }

    if (success) {
        start_accept();
    }
}

void meta_server::start_accept()
{
    new_connection.reset(new Connection(io_context, request_handler, signer, allowed_addreses));
    acceptor.async_accept(new_connection->socket, boost::bind(&meta_server::handle_accept, this, boost::asio::placeholders::error));
}

void meta_server::handle_accept(const boost::system::error_code& e)
{
    if (!e) {
        new_connection->start(new_connection);
    } else {
        //DEBUG_COUT("fail");
        //DEBUG_COUT(e.message());
    }

    start_accept();
}
void meta_server::update_allowed_addreses(std::unordered_set<std::string, crypto::Hasher> _allowed_addreses)
{
    allowed_addreses = std::move(_allowed_addreses);
}

}