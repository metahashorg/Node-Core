#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <set>
#include <unordered_map>

#include <concurrentqueue.h>
#include <open_ssl_decor.h>
#include <transaction.h>

#include <thread_pool.hpp>

#include "core_controller.hpp"

struct sha256_2_hasher {
    std::size_t operator()(const sha256_2& k) const
    {
        return get_xxhash64(k);
    }
};

struct Block;

class BlockChain;

struct ControllerImplementation {
    moodycamel::ConcurrentQueue<TX*> tx_queue;
    moodycamel::ConcurrentQueue<Block*> block_queue;
    moodycamel::ConcurrentQueue<ApproveRecord*> approve_queue;

    BlockChain* BC;
    ThreadPool& TP;

    std::vector<TX*> transactions;
    std::unordered_map<sha256_2, Block*, sha256_2_hasher> blocks;

    std::vector<char> PrivKey;
    std::vector<char> PubKey;
    std::string Addres;

    std::map<sha256_2, std::map<std::string, ApproveRecord*>> block_approve;
    std::map<sha256_2, std::map<std::string, ApproveRecord*>> block_disapprove;

    bool master = false;

    sha256_2 last_applied_block = { { 0 } };
    sha256_2 last_created_block = { { 0 } };
    sha256_2 proved_block = { { 0 } };

    uint64_t statistics_timestamp = 0;
    uint64_t prev_timestamp = 0;
    uint64_t prev_day = 0;
    uint64_t prev_state = 0;

    std::vector<RejectedTXInfo*> rejected_tx_list;
    uint64_t prev_rejected_ts = 0;

    std::string path;

    CoreController cores;
    uint64_t last_sync_timestamp = 0;
    uint64_t last_actualization_timestamp = 0;

    bool goon = true;
    const bool test = false;
    //    std::thread* p_thread_main_loop = nullptr;

public:
    ControllerImplementation(
        ThreadPool& TP,
        const std::string& priv_key_line,
        const std::string& _path,
        const std::string& proved_hash,
        const std::set<std::pair<std::string, int>>& core_list,
        const std::pair<std::string, int>& host_port,
        bool test);

    std::string add_pack_to_queue(std::string_view, std::string_view);

    std::string get_str_address();
    std::string get_last_block_str();

    std::atomic<std::map<std::string, std::pair<uint, uint>>*>& get_wallet_statistics();
    std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& get_wallet_request_addreses();

private:
    void main_loop();

    void parse_S_PING(std::string_view);
    void parse_B_TX(std::string_view);
    void parse_C_PRETEND_BLOCK(std::string_view);
    void parse_C_APPROVE(std::string_view);
    void parse_C_DISAPPROVE(std::string_view);
    void parse_C_APPROVE_BLOCK(std::string_view);
    std::string parse_S_LAST_BLOCK(std::string_view);
    std::string parse_S_GET_BLOCK(std::string_view);
    std::string parse_S_GET_CHAIN(std::string_view);
    std::string parse_S_GET_CORE_LIST(std::string_view);
    std::string parse_S_GET_CORE_ADDR(std::string_view);

    void approve_block(Block*);
    void disapprove_block(Block*);
    void apply_approve(ApproveRecord* p_ar);

    void distribute(Block*);
    void distribute(ApproveRecord*);
    //    void distribute(Block* block, ApproveRecord* p_ar);

    uint64_t min_approve();
    void write_block(Block*);
    bool try_make_block();

    void make_clean();

    void read_and_apply_local_chain();
    void start_main_loop();
    void actualize_chain();

    void apply_block_chain(
        std::map<sha256_2, Block*>& block_tree,
        std::map<sha256_2, Block*>& prev_tree,
        const std::string& source,
        bool need_write);
};
#endif // CONTROLLER_HPP
