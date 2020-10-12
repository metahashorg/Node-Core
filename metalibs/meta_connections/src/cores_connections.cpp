#include <meta_connections.hpp>

namespace metahash::connection {

MetaConnection::MetaConnection(boost::asio::io_context& io_context, const std::string& host, int port, crypto::Signer& signer, bool spectator)
    : io_context(io_context)
    , my_host(host)
    , my_port(port)
    , signer(signer)
    , spectator(spectator)
{
}

void MetaConnection::init(const std::map<std::string, std::pair<std::string, int>>& core_list)
{
    for (auto&& [mh_addr, host_port_pair] : core_list) {
        auto&& [host, port] = host_port_pair;
        cores.emplace(mh_addr, new network::meta_client(io_context, mh_addr, host, port, concurrent_connections_count, signer));
    }
}

bool MetaConnection::online(const std::string& addr)
{
    std::shared_lock lock(core_lock);
    if (cores.find(addr) != cores.end()) {
        return cores[addr]->online();
    } else {
        return false;
    }
}

std::set<std::string> MetaConnection::get_online_cores()
{
    std::set<std::string> online_cores;
    std::shared_lock lock(core_lock);
    for (auto&& [mh_addr, core] : cores) {
        if (core->online()) {
            online_cores.insert(mh_addr);
        }
    }
    online_cores.insert(signer.get_mh_addr());
    return online_cores;
}

std::set<std::string> MetaConnection::get_empty_queue_cores()
{
    std::set<std::string> empty_queue_cores;
    std::shared_lock lock(core_lock);
    for (auto&& [mh_addr, core] : cores) {
        if (core->get_queue_size() < 16) {
            empty_queue_cores.insert(mh_addr);
        }
    }
    return empty_queue_cores;
}

}