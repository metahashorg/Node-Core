#ifndef CHAIN_H
#define CHAIN_H

#include <atomic>
#include <set>

#include <meta_block.h>
#include <meta_pool.hpp>
#include <meta_wallet.h>

namespace metahash::meta_chain {

class BlockChain {
private:
    struct ProxyStat {
        std::map<std::string, std::pair<uint64_t, uint64_t>> stats;
        uint64_t count;
    };

    const std::map<std::string, std::string> test_nodes = {
        { "0x00ccbc94988be95731ce3ecdccca505fed5eac1f3498ad2966", "eu" },
        { "0x00b888869e8d4a193e80c59f923fe9f93fd6552875c857edbe", "us" },
        { "0x00c7343a54d1db1c6ece1302911f421775c6e1594b95a34126", "us" },
        { "0x00bc4787973cb36f47d4f274bc340cb3e1402030955c85e563", "cn" },
        { "0x00cacf8f42f4ffa95bc4a5eea3cf5986f56e13eed8ae012a67", "eu" }
    };

    sha256_2 prev_hash = { { 0 } };
    uint64_t state_hash_xx64 = 0;

    meta_wallet::WalletMap wallet_map;

    std::unordered_map<std::string, std::set<std::string>, crypto::Hasher> node_state;
    std::unordered_map<std::string, std::map<std::string, ProxyStat>, crypto::Hasher> node_statistics;

    std::vector<transaction::TX*> statistics_tx_list;
    std::vector<transaction::RejectedTXInfo*> rejected_tx_list;

    std::atomic<std::map<std::string, std::pair<uint, uint>>*> wallet_statistics = nullptr;
    std::atomic<std::deque<std::pair<std::string, uint64_t>>*> wallet_request_addreses = nullptr;

    boost::asio::io_context& io_context;

public:
    BlockChain(boost::asio::io_context& io_context);

    bool can_apply_block(block::Block* block);

    bool apply_block(block::Block* block);

    block::Block* make_forging_block(uint64_t timestamp);

    block::Block* make_state_block(uint64_t timestamp);

    block::Block* make_common_block(uint64_t timestamp, std::vector<transaction::TX*>& transactions);

    block::Block* make_statistics_block(uint64_t timestamp);

    std::vector<transaction::RejectedTXInfo*>* make_rejected_tx_block(uint64_t timestamp);

    std::atomic<std::map<std::string, std::pair<uint, uint>>*>& get_wallet_statistics();
    std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& get_wallet_request_addresses();

    std::set<std::string> check_addr(const std::string& addr);
    const std::unordered_map<std::string, std::set<std::string>, crypto::Hasher>& get_node_state();

private:
    static block::Block* make_block(uint64_t b_type, uint64_t b_time, sha256_2 prev_b_hash, std::vector<char>& tx_buff);

    static uint64_t get_fee(uint64_t cnt);
    static uint64_t FORGING_POOL(uint64_t ts);

    bool try_apply_block(block::Block* block, bool apply);
    bool can_apply_common_block(block::Block* block);
    bool can_apply_state_block(block::Block* block, bool);
    bool can_apply_forging_block(block::Block* block);
    void reject(const transaction::TX* tx, uint64_t reason);

    void fill_node_state();
};

}

#endif // CHAIN_H
