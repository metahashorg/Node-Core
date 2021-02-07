#include <meta_common.h>
//#include <meta_log.hpp>
#include <meta_server.h>

namespace metahash::network {

void Connection::write(std::shared_ptr<Connection> pThis)
{
    boost::asio::async_write(pThis->socket, pThis->reply.make_buff(), [pThis](const boost::system::error_code& e, std::size_t bytes_transferred) {
        if (!e) {
            if (pThis->reply.is_complete(bytes_transferred)) {
                pThis->start(pThis);
            } else {
                pThis->write(pThis);
            }
        } else {
            //DEBUG_COUT("error");
            //DEBUG_COUT(e.message());
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
    const auto public_key = _signer.get_pub_key();
    const auto sign = _signer.sign(message);
    uint32_t magic = METAHASH_MAGIC_NUMBER;

    write_buff.insert(write_buff.end(), reinterpret_cast<char*>(&magic), (reinterpret_cast<char*>(&magic) + sizeof(uint32_t)));
    crypto::append_varint(write_buff, reply_id);

    crypto::append_varint(write_buff, public_key.size());
    write_buff.insert(write_buff.end(), public_key.begin(), public_key.end());

    crypto::append_varint(write_buff, sign.size());
    write_buff.insert(write_buff.end(), sign.begin(), sign.end());

    crypto::append_varint(write_buff, message.size());
    write_buff.insert(write_buff.end(), message.begin(), message.end());
}

boost::asio::const_buffer Connection::Reply::make_buff()
{
    return boost::asio::const_buffer(write_buff.data() + offset, write_buff.size() - offset);
}

bool Connection::Reply::is_complete(uint64_t bytes_transferred)
{
    offset += bytes_transferred;
    return offset >= write_buff.size();
}

}