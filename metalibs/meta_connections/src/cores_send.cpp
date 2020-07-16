#include <meta_connections.hpp>

namespace metahash::connection {

void MetaConnection::send_no_return(uint64_t req_type, const std::vector<char>& req_data)
{
    std::lock_guard lock(core_lock);
    for (auto&& [mh_addr, core] : cores) {
        core->send_message(req_type, req_data, [](const std::vector<char>&) {});
    }
}

std::map<std::string, std::vector<char>> MetaConnection::send_with_return(uint64_t req_type, const std::vector<char>& req_data)
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

void MetaConnection::send_no_return_to_core(const std::string& addr, uint64_t req_type, const std::vector<char>& req_data)
{
    std::lock_guard lock(core_lock);
    if (cores.find(addr) != cores.end()) {
        cores[addr]->send_message(req_type, req_data, [](const std::vector<char>&) {});
    }
}

std::vector<char> MetaConnection::send_with_return_to_core(const std::string& addr, uint64_t req_type, const std::vector<char>& req_data)
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