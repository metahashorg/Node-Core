#include <meta_common.h>
#include <meta_log.hpp>
#include <meta_server.h>
#include <version.h>

namespace metahash::network {

namespace statics {
    std::vector<char> version_info(const std::string& mh_addr)
    {
        static const std::string version = std::string(VERSION_MAJOR) + "." + std::string(VERSION_MINOR) + "." + std::string(GIT_COUNT);
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

        return write_buff;
    }

    static const std::string unknown_sender = R"({"result":"error","error":"unknown sender"})";
    static const std::string invalid_sign = R"({"result":"error","error":"invalid sign"})";
}

Connection::Connection(boost::asio::io_context& io_context, std::function<std::vector<char>(Request&)> handler, crypto::Signer& signer, std::unordered_set<std::string, crypto::Hasher>& allowed_addreses)
    : serial_execution(io_context)
    , socket(serial_execution)
    , request_handler(std::move(handler))
    , signer(signer)
    , allowed_addreses(allowed_addreses)
{
}

void Connection::start(std::shared_ptr<Connection> pThis)
{
    pThis->request = {};
    pThis->reply = {};

    pThis->read(pThis);
}

void Connection::read(std::shared_ptr<Connection> pThis)
{
    pThis->socket.async_read_some(boost::asio::buffer(buffer), [pThis](const boost::system::error_code& e, std::size_t bytes_transferred) {
        if (!e) {
            auto result = static_cast<statics::parse_state>(pThis->request.parse(pThis->buffer.data(), bytes_transferred, pThis->allowed_addreses));

            switch (result) {
            case statics::SUCCESS: {
                pThis->request.remote_ip_address = pThis->socket.remote_endpoint().address().to_string();
                pThis->reply.make(pThis->signer, pThis->request.request_id, pThis->request_handler(pThis->request));

                pThis->write(pThis);
            } break;
            case statics::INCOMPLETE: {
                pThis->read(pThis);
            } break;
            case statics::WRONG_MAGIC_NUMBER: {
                // DEBUG_COUT("WRONG_MAGIC_NUMBER");
                pThis->reply.make_http(statics::version_info(pThis->signer.get_mh_addr()));
                pThis->write_and_close(pThis);
            } break;
            case statics::UNKNOWN_SENDER_METAHASH_ADDRESS: {
                // DEBUG_COUT("UNKNOWN_SENDER_METAHASH_ADDRESS");
                pThis->reply.make(pThis->signer, pThis->request.request_id, statics::unknown_sender);
                pThis->write_and_close(pThis);
            } break;
            case statics::INVALID_SIGN: {
                // DEBUG_COUT("INVALID_SIGN");
                pThis->reply.make(pThis->signer, pThis->request.request_id, statics::invalid_sign);
                pThis->write_and_close(pThis);
            } break;
            }
        } else {
            DEBUG_COUT("error");
            DEBUG_COUT(e.message());
        }
    });
}

}