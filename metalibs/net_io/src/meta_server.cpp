#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>
#include <utility>

#include <meta_log.hpp>
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

        return write_buff;
    }

    static const std::string unkown_sender = R"({"result":"error","error":"unkown sender"})";
    static const std::string invalid_sign = R"({"result":"error","error":"invalid sign"})";
}

meta_server::meta_server(boost::asio::io_context& io_context, const std::string& address, const int port, crypto::Signer& signer, std::function<std::vector<char>(Request&)> request_handler)
    : io_context(io_context)
    , endpoint(boost::asio::ip::tcp::v4(), static_cast<unsigned short>(port))
    , acceptor(io_context, endpoint)
    , request_handler(std::move(request_handler))
    , signer(signer)
    , new_connection()
{
    start();
}

void meta_server::start()
{
    acceptor.listen();

    start_accept();
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
        DEBUG_COUT("fail");
        DEBUG_COUT(e.message());
    }

    start_accept();
}
void meta_server::update_allowed_addreses(std::unordered_set<std::string, crypto::Hasher> _allowed_addreses)
{
    allowed_addreses = std::move(_allowed_addreses);
}

Connection::Connection(boost::asio::io_context& io_context, std::function<std::vector<char>(Request&)> handler, crypto::Signer& signer, std::unordered_set<std::string, crypto::Hasher>& allowed_addreses)
    : socket(io_context)
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
                DEBUG_COUT("WRONG_MAGIC_NUMBER");
                pThis->reply.make_http(statics::version_info(pThis->signer.get_mh_addr()));
                pThis->write_and_close(pThis);
            } break;
            case statics::UNKNOWN_SENDER_METAHASH_ADDRESS: {
                DEBUG_COUT("UNKNOWN_SENDER_METAHASH_ADDRESS");
                pThis->reply.make(pThis->signer, pThis->request.request_id, statics::unkown_sender);
                pThis->write_and_close(pThis);
            } break;
            case statics::INVALID_SIGN: {
                DEBUG_COUT("INVALID_SIGN");
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

void Connection::write(std::shared_ptr<Connection> pThis)
{
    boost::asio::async_write(pThis->socket, pThis->reply.make_buff(), [pThis](const boost::system::error_code& e, std::size_t bytes_transferred) {
//        DEBUG_COUT("write");
//        DEBUG_COUT(bytes_transferred);
        if (!e) {
            if (pThis->reply.is_complete(bytes_transferred)) {
                pThis->start(pThis);
            } else {
                pThis->write(pThis);
            }
        } else {
            DEBUG_COUT("error");
            DEBUG_COUT(e.message());
        }
    });
}

void Connection::write_and_close(std::shared_ptr<Connection> pThis)
{
    boost::asio::async_write(pThis->socket, pThis->reply.make_buff(), [pThis](const boost::system::error_code& e, std::size_t bytes_transferred) {
        if (!e) {
            if (pThis->reply.is_complete(bytes_transferred)) {
                boost::system::error_code ignored_ec;
                pThis->socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
            } else {
                pThis->write(pThis);
            }
        }
    });
}

void Connection::Reply::serialize(crypto::Signer& _signer, uint64_t reply_id, std::vector<char>& message)
{
    auto public_key = _signer.get_pub_key();
    auto sign = _signer.sign(message);
    uint32_t magic = METAHASH_MAGIC_NUMBER;

    write_buff.insert(write_buff.end(), reinterpret_cast<char*>(&magic), (reinterpret_cast<char*>(&magic) + sizeof(uint32_t)));
    crypto::append_varint(write_buff, reply_id);

    crypto::append_varint(write_buff, public_key.size());
    write_buff.insert(write_buff.end(), public_key.begin(), public_key.end());

    crypto::append_varint(write_buff, sign.size());
    write_buff.insert(write_buff.end(), sign.begin(), sign.end());

    crypto::append_varint(write_buff, message.size());
    write_buff.insert(write_buff.end(), message.begin(), message.end());

//    DEBUG_COUT(crypto::bin2hex(sign));
//    DEBUG_COUT(crypto::bin2hex(public_key));
//    DEBUG_COUT("reply_id\t" + std::to_string(reply_id));
//    DEBUG_COUT("message.size()\t" + std::to_string(message.size()));
}

boost::asio::const_buffer Connection::Reply::make_buff()
{
    return boost::asio::const_buffer(write_buff.data() + offset, write_buff.size() - offset);
}

bool Connection::Reply::is_complete(uint64_t bytes_transferred)
{
//    DEBUG_COUT("bytes_transferred\t" + std::to_string(bytes_transferred));
//    DEBUG_COUT("write_buff.size()\t" + std::to_string(write_buff.size()));
    offset += bytes_transferred;
    return offset >= write_buff.size();
}

bool Request::read_varint(uint64_t& varint)
{
    auto previous_offset = offset;
    offset += crypto::read_varint(varint, std::string_view(&request_full[offset], request_full.size() - offset));
    return offset != previous_offset;
}

bool Request::fill_sw(std::string_view& sw, uint64_t sw_size)
{
    if (offset + sw_size > request_full.size()) {
        return false;
    } else {
        sw = std::string_view(&request_full[offset], sw_size);
        offset += sw_size;
        return true;
    }
}

int8_t Request::parse(char* buff_data, size_t buff_size, std::unordered_set<std::string, crypto::Hasher>& allowed_addreses)
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
            sender_mh_addr = "0x" + crypto::bin2hex(crypto::get_address(public_key));
            if (!allowed_addreses.empty()) {
                if (allowed_addreses.find(sender_mh_addr) == allowed_addreses.end()) {
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

    if (sign.empty() && !fill_sw(sign, sign_size)) {
        return statics::INCOMPLETE;
    }

    if (message_size == 0 && !read_varint(message_size)) {
        return statics::INCOMPLETE;
    }
//    DEBUG_COUT(message_size);

    if (message_size && message.empty()) {
        if (fill_sw(message, message_size)) {
            if (!crypto::check_sign(message, sign, public_key)) {
                return statics::INVALID_SIGN;
            }
        } else {
            return statics::INCOMPLETE;
        }
    }
//    DEBUG_COUT(message.size());

    return statics::SUCCESS;
}
}