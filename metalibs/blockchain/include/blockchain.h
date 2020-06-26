#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

// STD LIBS
#include <atomic>

// Containers
#include <deque>
#include <map>
#include <set>
#include <string_view>

#include <thread_pool.hpp>

namespace metahash::metachain {
struct ControllerImplementation;
}

class BlockChainController {
private:
    metahash::metachain::ControllerImplementation* CI = nullptr;

public:
    BlockChainController(
        boost::asio::io_context& io_context,
        const std::string& priv_key_line,
        const std::string& path,
        const std::string& proved_hash,
        const std::map<std::string, std::pair<std::string, int>>& core_list,
        const std::pair<std::string, int>& host_port,
        bool test);

    std::atomic<std::map<std::string, std::pair<uint, uint>>*>& get_wallet_statistics();
    std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& get_wallet_request_addresses();

    ~BlockChainController();
};

#endif // BLOCKCHAIN_H
