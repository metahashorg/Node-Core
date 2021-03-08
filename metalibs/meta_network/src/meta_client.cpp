#include <meta_client.h>
#include <meta_common.h>
//#include <meta_log.hpp>

namespace metahash::network {

meta_client::meta_client(boost::asio::io_context& io_context, const std::string& mh_endpoint_addr, const std::string& server, const int port, const int max_connections, metahash::crypto::Signer& signer)
    : io_context(io_context)
    , resolver(io_context)
    , signer(signer)
    , mh_addr(mh_endpoint_addr)
    , server(server)
    , port(port)
    , max_connections(max_connections)
{
    resolve(server, std::to_string(port));
}


void meta_client::resolve(const std::string& host, const std::string& port)
{
    resolver.async_resolve(host, port, [this, host, port](boost::system::error_code err, const boost::asio::ip::tcp::resolver::results_type& results) {
        if (err) {            
            return resolve(host, port);
        }

        endpoints = results;

        sockets.reserve(max_connections);

        for (auto i = 0; i < max_connections; i++) {
            sockets.emplace_back(io_context, endpoints, tasks, mh_addr);
        }

        for (auto& conn : sockets) {
            conn.try_connect();
        }
    });
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

uint64_t meta_client::get_queue_size()
{
    return tasks.size_approx();
}

}
