#include <meta_connections.hpp>
#include <utility>

namespace metahash::connection {

void MetaConnection::send_no_return(uint64_t req_type, const std::vector<char>& req_data)
{
    std::shared_lock lock(core_lock);
    for (auto&& [mh_addr, core] : cores) {
        core->send_message(req_type, req_data, [](const std::vector<char>&) {});
    }
}

std::map<std::string, std::vector<char>> MetaConnection::send_with_return(uint64_t req_type, const std::vector<char>& req_data)
{
    std::map<std::string, std::vector<char>> resp_strings;
    std::map<std::string, std::future<std::vector<char>>> futures;

    {
        std::shared_lock lock(core_lock);
        for (auto&& [mh_addr, core] : cores) {
            auto promise = std::make_shared<std::promise<std::vector<char>>>();
            futures.insert({ mh_addr, promise->get_future() });

            core->send_message(req_type, req_data, [promise](const std::vector<char>& resp) {
                promise->set_value(resp);
            });
        }
    }

    uint64_t timestamp_deadline = 10 + static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count());

    for (auto&& [mh_addr, future] : futures) {
        uint64_t timestamp = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count());

        auto duration = timestamp > timestamp_deadline ? 0 : timestamp_deadline - timestamp;
        auto status = future.wait_for(std::chrono::seconds(duration));
        if (status == std::future_status::ready) {
            resp_strings[mh_addr] = future.get();
        }
    }

    return resp_strings;
}

void MetaConnection::send_with_callback(uint64_t req_type, const std::vector<char>& req_data, std::function<void(const std::string& mh_addr, const std::vector<char>&)> callback)
{
    std::shared_lock lock(core_lock);
    for (auto&& [mh_addr, core] : cores) {

        core->send_message(req_type, req_data, [mh_addr, callback](const std::vector<char>& resp) {
            callback(mh_addr, resp);
        });
    }
}

void MetaConnection::send_no_return_to_core(const std::string& addr, uint64_t req_type, const std::vector<char>& req_data)
{
    std::shared_lock lock(core_lock);
    if (cores.find(addr) != cores.end()) {
        cores[addr]->send_message(req_type, req_data, [](const std::vector<char>&) {});
    }
}

std::vector<char> MetaConnection::send_with_return_to_core(const std::string& addr, uint64_t req_type, const std::vector<char>& req_data)
{
    auto promise = std::make_shared<std::promise<std::vector<char>>>();
    auto future = promise->get_future();

    {
        std::shared_lock lock(core_lock);
        if (cores.find(addr) != cores.end()) {
            cores[addr]->send_message(req_type, req_data, [promise](const std::vector<char>& resp) {
                promise->set_value(resp);
            });
        }
    }

    auto status = future.wait_for(std::chrono::seconds(10));
    if (status == std::future_status::ready) {
        return future.get();
    }

    return std::vector<char>();
}

void MetaConnection::send_with_callback_to_one(const std::string& addr, uint64_t req_type, const std::vector<char>& req_data, std::function<void(const std::vector<char>&)> callback)
{
    std::shared_lock lock(core_lock);
    if (cores.find(addr) != cores.end()) {
        cores[addr]->send_message(req_type, req_data, callback);
    }
}

}