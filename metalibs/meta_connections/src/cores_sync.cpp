#include <meta_connections.hpp>
#include <meta_constants.hpp>
#include <meta_crypto.h>

namespace metahash::connection {

std::set<std::tuple<std::string, std::string, int>> parse_core_list(const std::vector<char>& data)
{
    std::string in_string(data.data(), data.size());
    std::set<std::tuple<std::string, std::string, int>> core_list;

    auto records = crypto::split(in_string, '\n');
    for (const auto& record : records) {
        auto host_port = crypto::split(record, ':');
        if (host_port.size() == 3) {
            core_list.insert({ host_port[0], host_port[1], atoi(host_port[2].c_str()) });
        }
    }

    return core_list;
}

void MetaConnection::sync_core_lists()
{
    bool got_new = true;
    while (got_new) {
        auto resp = send_with_return(RPC_GET_CORE_LIST, spectator ? std::vector<char>() : get_core_list());
        got_new = false;

        for (auto&& [mh_addr, data] : resp) {
            auto hosts = parse_core_list(data);

            add_new_cores(hosts);
        }
    }
}

void MetaConnection::add_cores(std::string_view pack)
{
    std::vector<char> data;
    data.insert(data.end(), pack.begin(), pack.end());
    auto hosts = parse_core_list(data);

    add_new_cores(hosts);
}

bool MetaConnection::add_new_cores(const std::set<std::tuple<std::string, std::string, int>>& hosts)
{
    bool got_new = false;
    std::unique_lock lock(core_lock);
    for (auto&& [addr, host, port] : hosts) {
        if (addr != signer.get_mh_addr() && port > 0) {
            if (cores.find(addr) == cores.end()) {
                cores.emplace(addr, new network::meta_client(io_context, addr, host, port, concurrent_connections_count, signer));
                got_new = true;
            }
        }
    }

    return got_new;
}

std::vector<char> MetaConnection::get_core_list()
{
    std::shared_lock lock(core_lock);

    std::vector<char> core_list;

    for (auto&& [addr, core] : cores) {
        auto&& [s_addr, s_host, i_port] = cores[addr]->get_definition();
        auto s_port = std::to_string(i_port);
        core_list.insert(core_list.end(), s_addr.begin(), s_addr.end());
        core_list.push_back(':');
        core_list.insert(core_list.end(), s_host.begin(), s_host.end());
        core_list.push_back(':');
        core_list.insert(core_list.end(), s_port.begin(), s_port.end());
        core_list.push_back('\n');
    }

    {
        auto s_addr = signer.get_mh_addr();
        auto s_host = my_host;
        auto s_port = std::to_string(my_port);

        core_list.insert(core_list.end(), s_addr.begin(), s_addr.end());
        core_list.push_back(':');
        core_list.insert(core_list.end(), s_host.begin(), s_host.end());
        core_list.push_back(':');
        core_list.insert(core_list.end(), s_port.begin(), s_port.end());
        core_list.push_back('\n');
    }

    return core_list;
}

}