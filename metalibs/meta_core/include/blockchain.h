#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <atomic>
#include <deque>
#include <map>

#include <meta_pool.hpp>

namespace metahash::meta_core {
struct ControllerImplementation;
}

class BlockChainController {
private:
    metahash::meta_core::ControllerImplementation* CI = nullptr;

public:
    BlockChainController(
        boost::asio::io_context& io_context,
        const std::string& priv_key_line,
        const std::string& path,
        const std::string& proved_hash,
        const std::map<std::string, std::pair<std::string, int>>& core_list,
        const std::pair<std::string, int>& host_port);

    std::atomic<std::map<std::string, std::pair<uint, uint>>*>& get_wallet_statistics();
    std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& get_wallet_request_addresses();

    ~BlockChainController();
};

#endif // BLOCKCHAIN_H
