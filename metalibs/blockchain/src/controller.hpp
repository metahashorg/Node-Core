#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <set>
#include <unordered_map>

#include <concurrentqueue.h>
#include <open_ssl_decor.h>
#include <transaction.h>

#include <meta_server.h>
#include <thread_pool.hpp>

#include "core_controller.hpp"

namespace metahash::metachain {

struct Block;

class BlockChain;

struct ControllerImplementation {
private:
    BlockChain* BC;
    boost::asio::io_context& io_context;
    boost::asio::io_context::strand serial_execution;
    boost::asio::deadline_timer main_loop_timer;

    std::vector<TX*> transactions;
    std::unordered_map<sha256_2, Block*, crypto::Hasher> blocks;

    std::unordered_map<sha256_2, std::map<std::string, ApproveRecord*>, crypto::Hasher> block_approve;
    std::unordered_map<sha256_2, std::map<std::string, ApproveRecord*>, crypto::Hasher> block_disapprove;

    sha256_2 last_applied_block = { { 0 } };
    sha256_2 last_created_block = { { 0 } };
    sha256_2 proved_block = { { 0 } };

    uint64_t min_approve = 0;

    uint64_t statistics_timestamp = 0;
    uint64_t prev_timestamp = 0;
    uint64_t prev_day = 0;
    uint64_t prev_state = 0;

    std::vector<RejectedTXInfo*> rejected_tx_list;
    uint64_t prev_rejected_ts = 0;

    const std::string path;

    crypto::Signer signer;

    CoreController cores;
    uint64_t last_sync_timestamp = 0;
    uint64_t last_actualization_timestamp = 0;

    net_io::meta_server *listener;

    bool goon = true;
public:
    ControllerImplementation(
        boost::asio::io_context& io_context,
        const std::string& priv_key_line,
        const std::string& _path,
        const std::string& proved_hash,
        const std::map<std::string, std::pair<std::string, int>>& core_list,
        const std::pair<std::string, int>& host_port,
        bool test);

    std::vector<char> add_pack_to_queue(net_io::Request& request);

    std::string get_str_address();
    std::string get_last_block_str();

    std::atomic<std::map<std::string, std::pair<uint, uint>>*>& get_wallet_statistics();
    std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& get_wallet_request_addresses();

private:
    void main_loop();

    void parse_S_PING(std::string_view);
    void parse_B_TX(std::string_view);
    void parse_C_PRETEND_BLOCK(std::string_view);
    void parse_C_APPROVE(std::string_view);
    void parse_C_DISAPPROVE(std::string_view);
    void parse_C_APPROVE_BLOCK(std::string_view);
    std::vector<char> parse_S_LAST_BLOCK(std::string_view);
    std::vector<char> parse_S_GET_BLOCK(std::string_view);
    std::vector<char> parse_S_GET_CHAIN(std::string_view);
    std::vector<char> parse_S_GET_CORE_LIST(std::string_view);
    std::vector<char> parse_S_GET_CORE_ADDR(std::string_view);

    void approve_block(Block*);
    void disapprove_block(Block*);
    void apply_approve(ApproveRecord* p_ar);

    void distribute(Block*);
    void distribute(ApproveRecord*);

    void write_block(Block*);
    bool try_make_block();

    void read_and_apply_local_chain();
    void actualize_chain();

    void apply_block_chain(
        std::unordered_map<sha256_2, Block*, crypto::Hasher>& block_tree,
        std::unordered_map<sha256_2, Block*, crypto::Hasher>& prev_tree,
        const std::string& source,
        bool need_write);

    void apply_block(Block* block);
    bool master();
};

}

#endif // CONTROLLER_HPP
