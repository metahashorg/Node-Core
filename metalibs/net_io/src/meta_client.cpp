#include <meta_client.h>
#include <meta_log.hpp>

namespace metahash::net_io {

namespace statics {
    enum parse_state {
        SUCCESS,
        INCOMPLETE,
        WRONG_MAGIC_NUMBER,
        UNKNOWN_SENDER_METAHASH_ADDRESS,
        INVALID_SIGN,
    };
}

int8_t ClientConnection::Response::parse(char* buff_data, size_t buff_size, const std::string& mh_endpoint_addr)
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

    if (public_key_size == 0 && !read_varint(public_key_size)) {
        return statics::INCOMPLETE;
    }

    if (public_key.empty()) {
        if (fill_sw(public_key, public_key_size)) {
            sender_addr = "0x" + crypto::bin2hex(crypto::get_address(public_key));
            if (mh_endpoint_addr != sender_addr) {
                DEBUG_COUT("UNKNOWN_SENDER_METAHASH_ADDRESS");
                return statics::UNKNOWN_SENDER_METAHASH_ADDRESS;
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

    if (message_size && message.empty()) {
        if (fill_sw(message, message_size)) {
            if (!crypto::check_sign(message, sign, public_key)) {
                DEBUG_COUT(crypto::bin2hex(sign));
                DEBUG_COUT(crypto::bin2hex(public_key));
                DEBUG_COUT("INVALID_SIGN");
                return statics::INVALID_SIGN;
            }
        } else {
            return statics::INCOMPLETE;
        }
    }

    return statics::SUCCESS;
}

bool ClientConnection::Response::read_varint(uint64_t& varint)
{
    auto previous_offset = offset;
    offset += crypto::read_varint(varint, std::string_view(&request_full[offset], request_full.size() - offset));
    return offset != previous_offset;
}

bool ClientConnection::Response::fill_sw(std::vector<char>& sw, uint64_t sw_size)
{
    if (offset + sw_size > request_full.size()) {
        return false;
    } else {
        sw.clear();
        sw.insert(sw.end(), &request_full[offset], &request_full[offset] + sw_size);
        offset += sw_size;
        return true;
    }
}

ClientConnection::ClientConnection(boost::asio::io_context& io_context, boost::asio::ip::basic_resolver<boost::asio::ip::tcp>::results_type& endpoints, moodycamel::ConcurrentQueue<Task*>& tasks, std::string mh_endpoint_addr)
    : mh_endpoint_addr(std::move(mh_endpoint_addr))
    , io_context(io_context)
    , endpoints(endpoints)
    , socket(new boost::asio::ip::tcp::socket(io_context))
    , tasks(tasks)
    , timer(io_context, boost::posix_time::milliseconds(10))
{
}

void ClientConnection::try_connect()
{
    boost::asio::async_connect(*socket, endpoints, [this](const boost::system::error_code& err, const boost::asio::ip::basic_endpoint<boost::asio::ip::tcp>&) {
        if (!err) {
            connected = true;
            check_tasks();
        } else {
            timer = boost::asio::deadline_timer(io_context, boost::posix_time::milliseconds(100));
            timer.async_wait([this](const boost::system::error_code&) {
                try_connect();
            });
        }
    });
}

void ClientConnection::check_tasks()
{
    if (tasks.try_dequeue(p_task) && p_task) {
        boost::asio::async_write(*socket, boost::asio::buffer(p_task->write_buff), [this](const boost::system::error_code& err, std::size_t bytes_transferred) {
            if (!err) {
                read();
            } else {
                DEBUG_COUT("error");
                DEBUG_COUT(err.message());

                reset();
            }
        });
    } else {
        timer = boost::asio::deadline_timer(io_context, boost::posix_time::milliseconds(10));
        timer.async_wait([this](const boost::system::error_code&) {
            check_tasks();
        });
    }
}

void ClientConnection::read()
{
    socket->async_read_some(boost::asio::buffer(buffer), [this](const boost::system::error_code& err, std::size_t bytes_transferred) {
        if (!err) {
            auto result = static_cast<statics::parse_state>(response.parse(buffer.data(), bytes_transferred, mh_endpoint_addr));

            switch (result) {
            case statics::SUCCESS: {
                std::vector<char> buff;
                buff.insert(buff.end(), response.message.begin(), response.message.end());

                p_task->callback(buff);

                response = {};
                delete p_task;
                p_task = nullptr;

                check_tasks();
            } break;
            case statics::INCOMPLETE: {
                read();
            } break;
            case statics::WRONG_MAGIC_NUMBER:
            case statics::UNKNOWN_SENDER_METAHASH_ADDRESS:
            case statics::INVALID_SIGN: {
                DEBUG_COUT("ERROR");
                p_task->callback(std::vector<char>());
                reset();
            } break;
            }

        } else {
            DEBUG_COUT("error");
            DEBUG_COUT(err.message());

            p_task->callback(std::vector<char>());
            reset();
        }
    });
}

void ClientConnection::reset()
{
    connected = false;
    socket.reset(new boost::asio::ip::tcp::socket(io_context));
    try_connect();
}
bool ClientConnection::online()
{
    return connected;
}

meta_client::meta_client(boost::asio::io_context& io_context, const std::string& mh_endpoint_addr, const std::string& server, const int port, const int max_connections, metahash::crypto::Signer& signer)
    : io_context(io_context)
    , resolver(io_context)
    , signer(signer)
    , mh_addr(mh_endpoint_addr)
    , server(server)
    , port(port)
{
    endpoints = resolver.resolve(server, std::to_string(port));

    sockets.reserve(max_connections);

    for (auto i = 0; i < max_connections; i++) {
        sockets.emplace_back(io_context, endpoints, tasks, mh_endpoint_addr);
    }

    for (auto& conn : sockets) {
        conn.try_connect();
    }
}

void meta_client::send_message(uint64_t request_type, const std::vector<char>& message, const std::function<void(std::vector<char>)>& callback)
{
    std::vector<char> write_buff;

    const uint64_t request_id = request_count++;
    uint64_t magic = METAHASH_MAGIC_NUMBER;

    const std::vector<char> public_key = signer.get_pub_key();
    const std::vector<char> sign = signer.sign(message);

    write_buff.insert(write_buff.end(), reinterpret_cast<char*>(&magic), (reinterpret_cast<char*>(&magic) + sizeof(uint32_t)));
    crypto::append_varint(write_buff, request_id);
    crypto::append_varint(write_buff, request_type);

    crypto::append_varint(write_buff, public_key.size());
    write_buff.insert(write_buff.end(), public_key.begin(), public_key.end());

    crypto::append_varint(write_buff, sign.size());
    write_buff.insert(write_buff.end(), sign.begin(), sign.end());

    crypto::append_varint(write_buff, message.size());
    write_buff.insert(write_buff.end(), message.begin(), message.end());

    tasks.enqueue(new Task { write_buff, callback });
}

std::tuple<std::string, std::string, int> meta_client::get_definition()
{
    return { mh_addr, server, port };
}

meta_client::~meta_client()
{
    Task* task = nullptr;
    while (tasks.try_dequeue(task)) {
        delete task;
    }
}

bool meta_client::online()
{
    for (auto& connection : sockets) {
        if (connection.online()) {
            return true;
        }
    }
    return false;
}

}
