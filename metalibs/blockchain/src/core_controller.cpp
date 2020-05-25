#include <utility>
#include <vector>

#include <meta_log.hpp>
#include <open_ssl_decor.h>

#include "core_controller.hpp"
#include "statics.hpp"

namespace metahash::metachain {

std::vector<std::string> split(const std::string& s, char delim)
{
    std::stringstream ss(s);
    std::string item;
    std::vector<std::string> elems;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::set<std::tuple<std::string, std::string, int>> parse_core_list(const std::vector<char>& data)
{
    std::string in_string(data.data(), data.size());
    std::set<std::tuple<std::string, std::string, int>> core_list;

    auto records = split(in_string, '\n');
    for (const auto& record : records) {
        auto host_port = split(record, ':');
        if (host_port.size() == 3) {
            core_list.insert({ host_port[0], host_port[1], std::stoi(host_port[2]) });
        }
    }

    return core_list;
}

CoreController::CoreController(boost::asio::io_context& io_context, const std::string& host, int port, const std::map<std::string, std::pair<std::string, int>>& core_list, crypto::Signer& signer)
    : io_context(io_context)
    , my_host(host)
    , my_port(port)
    , signer(signer)
{
    for (auto&& [mh_addr, host_port_pair] : core_list) {
        auto&& [host, port] = host_port_pair;
        cores.emplace(mh_addr, new net_io::meta_client(io_context, mh_addr, host, port, concurrent_connections_count, signer));
    }

    sync_core_lists();
}

void CoreController::sync_core_lists()
{
    bool got_new = true;
    while (got_new) {
        auto resp = send_with_return(RPC_GET_CORE_LIST, std::vector<char>());
        got_new = false;

        std::lock_guard lock(core_lock);
        for (auto&& [mh_addr, data] : resp) {
            auto hosts = parse_core_list(data);

            for (auto&& [addr, host, port] : hosts) {
                if (cores.find(addr) == cores.end()) {
                    got_new = true;
                    cores.emplace(addr, new net_io::meta_client(io_context, addr, host, port, concurrent_connections_count, signer));
                }
            }
        }
    }
}

void CoreController::add_core(std::string& add_addr, std::string& add_host, int add_port)
{
    std::lock_guard lock(core_lock);
    if (cores.find(add_addr) == cores.end()) {
        cores.emplace(add_addr, new net_io::meta_client(io_context, add_addr, add_host, add_port, concurrent_connections_count, signer));
    } else {
        auto&& [c_addr, c_host, c_port] = cores[add_addr]->get_definition();
        if (c_host != add_host || c_port != add_port) {
            delete cores[add_addr];
            cores[add_addr] = new net_io::meta_client(io_context, add_addr, add_host, add_port, concurrent_connections_count, signer);
        }
    }
}

std::vector<char> CoreController::get_core_list()
{
    std::lock_guard lock(core_lock);

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

    return core_list;
}

void CoreController::send_no_return(uint64_t req_type, const std::vector<char>& req_data)
{
    std::lock_guard lock(core_lock);
    for (auto&& [mh_addr, core] : cores) {
        core->send_message(req_type, req_data, [](const std::vector<char>&) {});
    }
}

std::map<std::string, std::vector<char>> CoreController::send_with_return(uint64_t req_type, const std::vector<char>& req_data)
{
    std::map<std::string, std::vector<char>> resp_strings;
    std::map<std::string, std::future<std::vector<char>>> futures;

    {
        std::lock_guard lock(core_lock);
        for (auto&& [mh_addr, core] : cores) {
            auto promise = std::make_shared<std::promise<std::vector<char>>>();
            futures.insert({ mh_addr, promise->get_future() });

            core->send_message(req_type, req_data, [promise](const std::vector<char>& resp) {
                promise->set_value(resp);
            });
        }
    }

    for (auto&& [mh_addr, future] : futures) {
        auto data = future.get();

        resp_strings[mh_addr] = data;
    }

    return resp_strings;
}

void CoreController::send_no_return_to_core(const std::string& addr, uint64_t req_type, const std::vector<char>& req_data)
{
    std::lock_guard lock(core_lock);
    if (cores.find(addr) != cores.end()) {
        cores[addr]->send_message(req_type, req_data, [](const std::vector<char>&) {});
    }
}

std::vector<char> CoreController::send_with_return_to_core(const std::string& addr, uint64_t req_type, const std::vector<char>& req_data)
{
    auto promise = std::make_shared<std::promise<std::vector<char>>>();
    auto future = promise->get_future();

    {
        std::lock_guard lock(core_lock);
        if (cores.find(addr) != cores.end()) {
            cores[addr]->send_message(req_type, req_data, [promise](const std::vector<char>& resp) {
                promise->set_value(resp);
            });
        }
    }

    return future.get();
}

}