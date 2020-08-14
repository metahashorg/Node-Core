#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <set>
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
    meta_chain::BlockChain* BC;
    boost::asio::io_context& io_context;
    boost::asio::io_context::strand serial_execution;
    boost::asio::deadline_timer main_loop_timer;

    std::vector<transaction::TX*> transactions;
    std::unordered_map<sha256_2, block::Block*, crypto::Hasher> blocks;

    std::unordered_map<sha256_2, std::map<std::string, transaction::ApproveRecord*>, crypto::Hasher> block_approve;
    std::unordered_map<sha256_2, std::map<std::string, transaction::ApproveRecord*>, crypto::Hasher> block_disapprove;

    sha256_2 last_applied_block = { { 0 } };
    sha256_2 last_created_block = { { 0 } };
    sha256_2 proved_block = { { 0 } };

    uint64_t min_approve = 0;

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

    network::meta_server* listener;

    std::vector<std::string> current_cores;
    std::map<uint64_t, std::unordered_map<std::string, std::set<std::string>, crypto::Hasher>> proposed_cores;
    uint64_t core_list_generation = 0;

    bool goon = true;

    uint64_t dbg_timestamp = 0;
    std::atomic<uint64_t> dbg_RPC_PING = 0;
    std::atomic<uint64_t> dbg_RPC_TX = 0;
    std::atomic<uint64_t> dbg_RPC_GET_CORE_LIST = 0;
    std::atomic<uint64_t> dbg_RPC_APPROVE = 0;
    std::atomic<uint64_t> dbg_RPC_DISAPPROVE = 0;
    std::atomic<uint64_t> dbg_RPC_LAST_BLOCK = 0;
    std::atomic<uint64_t> dbg_RPC_GET_BLOCK = 0;
    std::atomic<uint64_t> dbg_RPC_GET_CHAIN = 0;
    std::atomic<uint64_t> dbg_RPC_CORE_LIST_APPROVE = 0;
    std::atomic<uint64_t> dbg_RPC_PRETEND_BLOCK = 0;
    std::atomic<uint64_t> dbg_RPC_NONE = 0;

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

    std::vector<char> add_pack_to_queue(network::Request& request);
    void parse_S_PING(std::string_view);
    void parse_B_TX(std::string_view);
    void parse_C_PRETEND_BLOCK(std::string_view);
    void parse_C_APPROVE(std::string_view);
    void parse_C_DISAPPROVE(std::string_view);
    std::vector<char> parse_S_LAST_BLOCK(std::string_view);
    std::vector<char> parse_S_GET_BLOCK(std::string_view);
    std::vector<char> parse_S_GET_CHAIN(std::string_view);
    std::vector<char> parse_S_GET_CORE_LIST(std::string_view);
    void parse_S_CORE_LIST_APPROVE(std::string core, std::string_view);

    void approve_block(block::Block*);
    void disapprove_block(block::Block*);
    void apply_approve(transaction::ApproveRecord*);

    void distribute(block::Block*);
    void distribute(transaction::ApproveRecord*);

    void write_block(block::Block*);
    bool try_make_block();

    void read_and_apply_local_chain();
    void actualize_chain();
    void check_if_chain_actual();

    void apply_block_chain(
        std::unordered_map<sha256_2, block::Block*, crypto::Hasher>& block_tree,
        std::unordered_map<sha256_2, block::Block*, crypto::Hasher>& prev_tree,
        const std::string& source,
        bool need_write);

    void try_apply_block(block::Block*);
    bool master();
    bool check_online_nodes();
    std::vector<char> make_pretend_core_list();
};

}

#endif // CONTROLLER_HPP
