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

struct ControllerImplementation;

class BlockChainController {
private:
    ControllerImplementation* CI = nullptr;

public:
    BlockChainController(
        const std::string& priv_key_line,
        const std::string& path,
        const std::string& proved_hash,
        const std::set<std::pair<std::string, int>>& core_list,
        const std::pair<std::string, int>& host_port);

    std::string add_pack_to_queue(std::string_view, std::string_view);

    std::string get_str_address();
    std::string get_last_block_str();

    std::atomic<std::map<std::string, std::pair<int, int>>*>& get_wallet_statistics();
    std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& get_wallet_request_addreses();

    ~BlockChainController();
};

#endif // BLOCKCHAIN_H
