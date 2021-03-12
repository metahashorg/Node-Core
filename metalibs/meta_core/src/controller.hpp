#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <set>
#include <shared_mutex>
#include <unordered_map>

#include <meta_block.h>
#include <meta_chain.h>
#include <meta_connections.hpp>
#include <meta_crypto.h>
#include <meta_pool.hpp>
#include <meta_server.h>
#include <meta_transaction.h>

namespace metahash::meta_core {

struct ControllerImplementation {
private:
    meta_chain::BlockChain BC;
    boost::asio::io_context& io_context;
    boost::asio::io_context::strand serial_execution;
    boost::asio::deadline_timer main_loop_timer;

    std::vector<transaction::TX*> transactions;

    std::map<sha256_2, std::set<std::string>> missing_blocks;

    std::unordered_map<sha256_2, std::map<std::string, transaction::ApproveRecord*>, crypto::Hasher> block_approve;
    std::unordered_map<sha256_2, std::map<std::string, transaction::ApproveRecord*>, crypto::Hasher> block_disapprove;

    moodycamel::ConcurrentQueue<transaction::TX*> tx_queue;
    moodycamel::ConcurrentQueue<transaction::ApproveRecord*> approve_queue;
    moodycamel::ConcurrentQueue<block::Block*> block_queue;
    moodycamel::ConcurrentQueue<std::pair<std::string, sha256_2>> approve_request_queue;

    sha256_2 last_applied_block = { { 0 } };
    sha256_2 last_created_block = { { 0 } };
    sha256_2 proved_block = { { 0 } };

    const uint64_t min_approve = 0;

    uint64_t statistics_timestamp = 0;
    uint64_t prev_timestamp = 0;
    uint64_t prev_day = 0;
    uint64_t prev_state = 0;

    std::vector<transaction::RejectedTXInfo*> rejected_tx_list;
    uint64_t prev_rejected_ts = 0;

    const std::string path;

    crypto::Signer signer;

    connection::MetaConnection cores;
    uint64_t last_sync_timestamp = 0;

    uint64_t last_actualization_timestamp = 0;
    uint64_t last_actualization_check_timestamp = 0;
    uint64_t actualization_iteration = 0;
    int not_actualized[2];

    network::meta_server listener;

    std::unordered_map<std::string, uint64_t, crypto::Hasher> core_last_block;

    std::vector<std::string> current_cores;
    std::map<uint64_t, std::unordered_map<std::string, std::set<std::string>, crypto::Hasher>> proposed_cores;
    uint64_t core_list_generation = 0;
    uint64_t generation_check_timestamp = 0;

    bool goon = true;

    struct Statistics {
        std::atomic<uint64_t> dbg_RPC_TX = 0;
        std::atomic<uint64_t> dbg_RPC_GET_CORE_LIST = 0;
        std::atomic<uint64_t> dbg_RPC_APPROVE = 0;
        std::atomic<uint64_t> dbg_RPC_DISAPPROVE = 0;
        std::atomic<uint64_t> dbg_RPC_GET_APPROVE = 0;
        std::atomic<uint64_t> dbg_RPC_APPROVE_LIST = 0;
        std::atomic<uint64_t> dbg_RPC_LAST_BLOCK = 0;
        std::atomic<uint64_t> dbg_RPC_GET_BLOCK = 0;
        std::atomic<uint64_t> dbg_RPC_GET_CHAIN = 0;
        std::atomic<uint64_t> dbg_RPC_GET_MISSING_BLOCK_LIST = 0;
        std::atomic<uint64_t> dbg_RPC_CORE_LIST_APPROVE = 0;
        std::atomic<uint64_t> dbg_RPC_PRETEND_BLOCK = 0;
        std::atomic<uint64_t> dbg_RPC_NONE = 0;

        std::shared_mutex ip_lock;
        std::set<std::string> ip;
    };

    std::shared_mutex income_nodes_stat_lock;
    std::unordered_map<std::string, Statistics, crypto::Hasher> income_nodes_stat;
    uint64_t dbg_timestamp = 0;

    struct Blocks {
        std::shared_mutex blocks_lock;
        std::unordered_map<sha256_2, block::Block*, crypto::Hasher> blocks;
        std::unordered_map<sha256_2, block::Block*, crypto::Hasher> previous;

        bool contains(const sha256_2&);
        bool contains_next(const sha256_2&);
        void insert(block::Block*);
        block::Block* operator[](const sha256_2&);
        block::Block* get_next(const sha256_2&);
        void erase(const sha256_2&);

    } blocks;

public:
    ControllerImplementation(
        boost::asio::io_context& io_context,
        const std::string& priv_key_line,
        const std::string& _path,
        const std::string& proved_hash,
        const std::map<std::string, std::pair<std::string, int>>& core_list,
        const std::pair<std::string, int>& host_port);

    std::atomic<std::map<std::string, std::pair<uint, uint>>*>& get_wallet_statistics();
    std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& get_wallet_request_addresses();

private:
    void main_loop();
    void process_queues();
    bool check_if_can_make_block(const uint64_t& timestamp);
    bool check_awaited_blocks();

    std::vector<char> add_pack_to_queue(network::Request& request);
    void log_network_statistics(uint64_t timestamp);

    void parse_RPC_TX(std::string_view);
    void parse_RPC_PRETEND_BLOCK(std::string_view);
    void parse_RPC_APPROVE(std::string_view);
    void parse_RPC_DISAPPROVE(std::string_view);
    void parse_RPC_APPROVE_LIST(std::string_view);
    void parse_RPC_GET_APPROVE(const std::string& core, std::string_view);
    std::vector<char> parse_RPC_LAST_BLOCK(std::string_view);
    std::vector<char> parse_RPC_GET_BLOCK(std::string_view);
    std::vector<char> parse_RPC_GET_MISSING_BLOCK_LIST(std::string_view);
    std::vector<char> parse_RPC_GET_CORE_LIST(std::string_view);
    void parse_RPC_CORE_LIST_APPROVE(const std::string& core, std::string_view);

    void approve_block(block::Block*);
    void disapprove_block(block::Block*);
    void apply_approve(transaction::ApproveRecord*);
    bool count_approve_for_block(block::Block*);
    bool try_apply_block(block::Block*, bool write = true);
    void distribute(block::Block*);
    void distribute(transaction::ApproveRecord*);
    bool master();
    bool check_block_for_appliance_and_break_on_corrupt_block(block::Block*& block);

    void write_block(block::Block*);

    bool try_make_block(uint64_t timestamp);

    void read_and_apply_local_chain();
    void check_blocks();

    void check_if_chain_actual();
    void actualize_chain();

    void get_approve_for_block(sha256_2& block_hash);
    void get_approve_for_block(std::vector<char>& get_block);

    bool check_online_nodes(uint64_t timestamp);
    std::vector<char> make_pretend_core_list(uint64_t current_generation);
};

}

#endif // CONTROLLER_HPP
