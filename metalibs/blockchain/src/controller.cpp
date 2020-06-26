#include "controller.hpp"

#include <blockchain.h>
#include <meta_log.hpp>
#include <open_ssl_decor.h>
#include <statics.hpp>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "block.h"
#include "chain.h"

#include <cmath>
#include <fstream>
#include <future>
#include <list>
#include <random>

namespace metahash::metachain {

ControllerImplementation::ControllerImplementation(
    boost::asio::io_context& io_context,
    const std::string& priv_key_line,
    const std::string& path,
    const std::string& proved_hash,
    const std::map<std::string, std::pair<std::string, int>>& core_list,
    const std::pair<std::string, int>& host_port,
    bool test)
    : BC(new BlockChain(io_context))
    , io_context(io_context)
    , serial_execution(io_context)
    , main_loop_timer(io_context, boost::posix_time::milliseconds(10))
    , min_approve(std::ceil(METAHASH_PRIMARY_CORES_COUNT * 51.0 / 100.0))
    , path(path)
    , signer(crypto::hex2bin(priv_key_line))
    , cores(io_context, host_port.first, host_port.second, signer)
{
    DEBUG_COUT("min_approve\t" + std::to_string(min_approve));

    {
        current_cores.insert(current_cores.end(), FOUNDER_WALLETS.begin(), FOUNDER_WALLETS.end());
    }

    {
        std::vector<unsigned char> bin_proved_hash = crypto::hex2bin(proved_hash);
        std::copy_n(bin_proved_hash.begin(), 32, proved_block.begin());
    }

    read_and_apply_local_chain();

    serial_execution.post([this] {
        main_loop();
    });

    listener = new net_io::meta_server(
        io_context, host_port.first, host_port.second, signer, [this](net_io::Request& request) -> std::vector<char> {
            return add_pack_to_queue(request);
        });

    cores.init(core_list);
}

void ControllerImplementation::apply_block_chain(std::unordered_map<sha256_2, Block*, crypto::Hasher>& block_tree,
    std::unordered_map<sha256_2, Block*, crypto::Hasher>& prev_tree,
    const std::string& source, bool need_write)
{
    static const sha256_2 zero_block = { { 0 } };

    bool do_clean_and_return = false;
    if (last_applied_block == zero_block) {
        if (proved_block == zero_block) {
            if (!prev_tree.count(zero_block)) {
                DEBUG_COUT("blockchain from " + source + " have no blocks to connect with my chain to zero_block\t" + crypto::bin2hex(zero_block));
                do_clean_and_return = true;
            }
        } else if (!block_tree.count(proved_block)) {
            DEBUG_COUT("blockchain from " + source + " have no blocks to connect with my chain to proved_block\t" + crypto::bin2hex(proved_block));
            do_clean_and_return = true;
        }
    } else if (!prev_tree.count(last_applied_block)) {
        DEBUG_COUT("blockchain from " + source + " have no blocks to connect with my chain to last_applied_block\t" + crypto::bin2hex(last_applied_block));
        do_clean_and_return = true;
    }

    if (do_clean_and_return) {
        for (auto& map_pair : block_tree) {
            delete map_pair.second;
        }
        return;
    }

    std::list<Block*> block_chain;

    if (last_applied_block == zero_block && proved_block != zero_block) {
        sha256_2 curr_block = proved_block;

        bool got_start = false;
        while (block_tree.count(curr_block)) {
            Block* block = block_tree[curr_block];
            block_chain.push_front(block);
            if (block->get_block_type() == BLOCK_TYPE_STATE || block->get_prev_hash() == zero_block) {
                got_start = true;
                break;
            }
            curr_block = block->get_prev_hash();
        }

        if (!got_start) {
            if (need_write) {
                for (auto& map_pair : block_tree) {
                    delete map_pair.second;
                }
            }
            DEBUG_COUT("blockchain from " + source + " is incomplete");
            return;
        }
    }

    {
        sha256_2 curr_block = last_applied_block != zero_block ? last_applied_block : proved_block;

        while (prev_tree.count(curr_block)) {
            Block* block = prev_tree[curr_block];
            block_chain.push_back(block);
            curr_block = block->get_block_hash();
        }
    }

    for (auto block : block_chain) {
        if (!BC->apply_block(block)) {
            DEBUG_COUT("blockchain from " + source + " have have errors in block " + crypto::bin2hex(block->get_block_hash()));
            break;
        }

        if (need_write) {
            uint64_t timestamp = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count());

            if (timestamp - block->get_block_timestamp() < 60) {
                auto* p_ar = new ApproveRecord;
                p_ar->make(block->get_block_hash(), signer);
                p_ar->approve = true;
                distribute(p_ar);
                delete p_ar;
            }

            write_block(block);
            blocks.insert({ last_applied_block, block });
        }

        prev_timestamp = block->get_block_timestamp();
        if (block->get_block_type() == BLOCK_TYPE_STATE) {
            prev_day = prev_timestamp / DAY_IN_SECONDS;
        }

        last_applied_block = block->get_block_hash();
        last_created_block = block->get_block_hash();
    }

    DEBUG_COUT("START");
    if (need_write) {
        std::atomic<int> jobs = 0;
        for (auto&& [hash, block] : block_tree) {
            jobs++;
            boost::asio::post(io_context, [hash, block, this, &jobs]() {
                if (!blocks.count(hash)) {
                    delete block;
                }
                jobs--;
            });
        }
        while (jobs.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    DEBUG_COUT("STOP");
}

void ControllerImplementation::check_if_chain_actual()
{
    DEBUG_COUT("check_if_chain_actual");

    std::map<std::string, std::vector<char>> last_block_on_cores = cores.send_with_return(RPC_LAST_BLOCK, std::vector<char>());

    std::set<std::string> cores_with_missing_block;
    std::set<sha256_2> missing_blocks;

    serial_execution.post([this, last_block_on_cores] {
        bool need_actualization = false;
        for (auto&& [core_addr, block_info] : last_block_on_cores) {
            if (block_info.size() >= 40) {
                sha256_2 last_block_return;
                std::copy_n(block_info.begin(), 32, last_block_return.begin());

                uint64_t block_timestamp = 0;
                std::copy_n(block_info.begin() + 32, 8, reinterpret_cast<char*>(&block_timestamp));

                if (block_timestamp > prev_timestamp && !blocks.count(last_block_return)) {
                    DEBUG_COUT("core\t" + core_addr + "have more recent block\t" + crypto::bin2hex(last_block_return));
                    need_actualization = true;
                }
            }
        }
        if (need_actualization) {
            DEBUG_COUT("need_actualization");
            actualize_chain();
        }
    });
}

void ControllerImplementation::actualize_chain()
{
    DEBUG_COUT("actualize_chain");

    std::map<std::string, std::vector<char>> last_block_on_cores = cores.send_with_return(RPC_LAST_BLOCK, std::vector<char>());

    std::set<std::string> cores_with_missing_block;
    std::set<sha256_2> missing_blocks;
    for (auto&& [core_addr, block_info] : last_block_on_cores) {
        if (block_info.size() >= 40) {
            sha256_2 last_block_return;
            std::copy_n(block_info.begin(), 32, last_block_return.begin());

            uint64_t block_timestamp = 0;
            std::copy_n(block_info.begin() + 32, 8, reinterpret_cast<char*>(&block_timestamp));

            if (block_timestamp > prev_timestamp && !blocks.count(last_block_return)) {
                DEBUG_COUT("core\t" + core_addr + "have more recent block\t" + crypto::bin2hex(last_block_return));
                cores_with_missing_block.insert(core_addr);
                missing_blocks.insert(last_block_return);
            }
        }
    }

    if (!cores_with_missing_block.empty()) {
        DEBUG_COUT("GET BLOCKCHAIN START");

        std::unordered_map<sha256_2, Block*, crypto::Hasher> block_tree;
        std::unordered_map<sha256_2, Block*, crypto::Hasher> prev_tree;

        uint64_t failed = 0;

        for (;;) {
            for (const auto& core : cores_with_missing_block) {
                if (missing_blocks.empty() || failed > 100) {
                    DEBUG_COUT("GET BLOCKCHAIN COMPLETE");
                    apply_block_chain(block_tree, prev_tree, "metachain network", true);
                    DEBUG_COUT("APPLY BLOCKCHAIN COMPLETE");
                    return;
                }

                sha256_2 requested_block = *missing_blocks.begin();

                std::vector<char> block_as_vector;
                block_as_vector.insert(block_as_vector.end(), requested_block.begin(), requested_block.end());

                DEBUG_COUT("trying to get block\t" + crypto::bin2hex(requested_block) + "\tfrom:\t" + core);
                std::vector<char> return_data = cores.send_with_return_to_core(core, RPC_GET_BLOCK, block_as_vector);

                if (return_data.empty()) {
                    DEBUG_COUT("Block is empty");
                } else {
                    missing_blocks.erase(missing_blocks.begin());
                }

                std::string_view block_as_sw(return_data.data(), return_data.size());
                Block* block = parse_block(block_as_sw);
                if (block) {
                    if (dynamic_cast<CommonBlock*>(block)) {
                        failed = 0;
                        if (!block_tree.insert({ block->get_block_hash(), block }).second) {
                            DEBUG_COUT("Duplicate block in chain\t" + crypto::bin2hex(block->get_block_hash()));
                            delete block;
                            block = nullptr;
                        } else if (!prev_tree.insert({ block->get_prev_hash(), block }).second) {
                            DEBUG_COUT("Branches in block chain\t" + crypto::bin2hex(block->get_prev_hash()) + "\t->\t" + crypto::bin2hex(block->get_block_hash()));
                            block_tree.erase(block->get_block_hash());
                            delete block;
                            block = nullptr;
                        } else if (!blocks.count(block->get_prev_hash())) {
                            if (block->get_prev_hash() == sha256_2 {}) {
                                DEBUG_COUT("Got complete chain. Previous block is zero block");
                            } else {
                                missing_blocks.insert(block->get_prev_hash());
                            }
                        }
                    } else {
                        failed++;
                        delete block;
                        block = nullptr;
                    }
                } else {
                    failed++;
                    DEBUG_COUT("Parse block error");
                }
            }
        }
    }
}

std::string ControllerImplementation::get_str_address()
{
    return signer.get_mh_addr();
}

std::string ControllerImplementation::get_last_block_str()
{
    return crypto::bin2hex(last_applied_block);
}

std::atomic<std::map<std::string, std::pair<uint, uint>>*>& ControllerImplementation::get_wallet_statistics()
{
    return BC->get_wallet_statistics();
}

std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& ControllerImplementation::get_wallet_request_addresses()
{
    return BC->get_wallet_request_addresses();
}

void ControllerImplementation::main_loop()
{
    uint64_t timestamp = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count());
    bool no_sleep = false;

    if (timestamp - last_sync_timestamp > 60) {
        DEBUG_COUT("sync_core_lists");
        io_context.post([this] { cores.sync_core_lists(); });

        last_sync_timestamp = timestamp;
    }

    if (check_online_nodes()) {
        {
            if (prev_timestamp > last_actualization_timestamp) {
                last_actualization_timestamp = prev_timestamp;
            }

            uint64_t sync_interval = master() ? 60 : 1;

            if (timestamp - last_actualization_timestamp > sync_interval) {
                last_actualization_timestamp = timestamp;
                io_context.post([this] {
                    check_if_chain_actual();
                });

                no_sleep = true;
            }
        }

        for (auto&& [hash, block] : blocks) {
            if (!block_approve[hash].count(signer.get_mh_addr()) && !block_disapprove[hash].count(signer.get_mh_addr())) {
                if (block->get_prev_hash() == last_applied_block) {
                    if (BC->can_apply_block(block)) {
                        approve_block(block);
                    } else {
                        disapprove_block(block);
                    }

                    no_sleep = true;
                }
            }
        }

        if (master() && try_make_block()) {
            no_sleep = true;
        }
    }

    if (no_sleep) {
        serial_execution.post([this] {
            main_loop();
        });
    } else {
        main_loop_timer = boost::asio::deadline_timer(io_context, boost::posix_time::milliseconds(1));
        main_loop_timer.async_wait([this](const boost::system::error_code&) {
            serial_execution.post([this] {
                main_loop();
            });
        });
    }
}

void ControllerImplementation::approve_block(Block* p_block)
{
    auto* p_ar = new ApproveRecord;
    p_ar->make(p_block->get_block_hash(), signer);
    p_ar->approve = true;

    distribute(p_ar);
    apply_approve(p_ar);
}

void ControllerImplementation::disapprove_block(Block* p_block)
{
    auto* p_ar = new ApproveRecord;
    p_ar->make(p_block->get_block_hash(), signer);
    p_ar->approve = false;

    distribute(p_ar);
    apply_approve(p_ar);
}

void ControllerImplementation::apply_approve(ApproveRecord* p_ar)
{
    sha256_2 block_hash;
    std::copy(p_ar->block_hash.begin(), p_ar->block_hash.end(), block_hash.begin());

    std::string addr = "0x" + crypto::bin2hex(crypto::get_address(p_ar->pub_key));

    if (p_ar->approve) {
        if (!block_approve[block_hash].insert({ addr, p_ar }).second) {
            DEBUG_COUT("APPROVE ALREADY PRESENT NOT CREATED");
            delete p_ar;
        }

        uint64_t approve_size = 0;
        for (auto&& [addr, _] : block_approve[block_hash]) {
            if (std::find(current_cores.begin(), current_cores.end(), addr) != std::end(current_cores)) {
                approve_size++;
            }
        }

        if (approve_size >= min_approve) {
            if (blocks.count(block_hash)) {
                Block* block = blocks[block_hash];
                if (block->get_prev_hash() == last_applied_block) {
                    try_apply_block(block);
                } else {
                    DEBUG_COUT("block->get_prev_hash != last_applied_block");
                }
            } else {
                DEBUG_COUT("!blocks.contains(block_hash)");
            }
        }
    } else {
        if (!block_disapprove[block_hash].insert({ addr, p_ar }).second) {
            delete p_ar;
        }
    }
}

void ControllerImplementation::try_apply_block(Block* block)
{
    if (BC->apply_block(block)) {
        write_block(block);

        if (block->get_block_type() == BLOCK_TYPE_STATE) {
            std::unordered_set<std::string, crypto::Hasher> allowed_addresses;
            for (auto&& [addr, roles] : BC->get_node_state()) {
                if (roles.count(META_ROLE_CORE) || roles.count(META_ROLE_VERIF)) {
                    allowed_addresses.insert(addr);
                }
            }
            listener->update_allowed_addreses(allowed_addresses);
        }
    } else {
        DEBUG_COUT("!BC->can_apply_block(block)");
    }
}

void ControllerImplementation::distribute(Block* block)
{
    std::vector<char> send_pack;
    send_pack.insert(send_pack.end(), block->get_data().begin(), block->get_data().end());

    cores.send_no_return(RPC_PRETEND_BLOCK, send_pack);
}

void ControllerImplementation::distribute(ApproveRecord* p_ar)
{
    auto approve_str = p_ar->approve ? RPC_APPROVE : RPC_DISAPPROVE;
    std::vector<char> send_pack;
    send_pack.insert(send_pack.end(), p_ar->data.begin(), p_ar->data.end());

    cores.send_no_return(approve_str, send_pack);
}

bool ControllerImplementation::try_make_block()
{
    uint64_t timestamp = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count());
    if (last_applied_block == last_created_block) {
        if (prev_timestamp >= timestamp) {
            return false;
        }

        uint64_t block_state = BLOCK_TYPE_COMMON;

        uint64_t current_day = (timestamp + 1) / DAY_IN_SECONDS;

        if (prev_state == BLOCK_TYPE_FORGING) {
            block_state = BLOCK_TYPE_STATE;
            DEBUG_COUT("BLOCK_STATE_STATE");
        } else {
            if (current_day > prev_day && prev_state != BLOCK_TYPE_FORGING) {
                block_state = BLOCK_TYPE_FORGING;
                DEBUG_COUT("BLOCK_STATE_FORGING");
            } else {
                block_state = BLOCK_TYPE_COMMON;
            }
        }

        {
            Block* block = nullptr;
            switch (block_state) {
            case BLOCK_TYPE_COMMON:
                if (timestamp - statistics_timestamp > 600) {
                    block = BC->make_statistics_block(timestamp);
                    statistics_timestamp = timestamp;
                } else {
                    if (!transactions.empty()) {
                        block = BC->make_common_block(timestamp, transactions);
                    }
                }
                break;
            case BLOCK_TYPE_FORGING:
                DEBUG_COUT("BLOCK_TYPE_FORGING");
                block = BC->make_forging_block(timestamp);
                break;
            case BLOCK_TYPE_STATE:
                DEBUG_COUT("BLOCK_TYPE_STATE");
                block = BC->make_state_block(timestamp);
                break;
            }

            if (block) {
                if (BC->can_apply_block(block)) {
                    last_created_block = block->get_block_hash();
                    blocks.insert({ block->get_block_hash(), block });
                    distribute(block);
                    approve_block(block);
                    return true;
                }

                DEBUG_COUT("FUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU");
                std::this_thread::sleep_for(std::chrono::seconds(3));
                exit(1);
            }
        }

        if (auto tx_list = BC->make_rejected_tx_block(timestamp)) {
            rejected_tx_list.insert(rejected_tx_list.end(), tx_list->begin(), tx_list->end());
            delete tx_list;

            auto reject_tx_block = new RejectedTXBlock;
            if (!rejected_tx_list.empty() && prev_rejected_ts != timestamp && reject_tx_block->make(timestamp, last_applied_block, rejected_tx_list, signer)) {
                prev_rejected_ts = timestamp;
                distribute(reject_tx_block);
                write_block(reject_tx_block);
                for (auto* tx : rejected_tx_list) {
                    delete tx;
                }
                rejected_tx_list.clear();
            }
            delete reject_tx_block;
        }
    } else if (timestamp - prev_timestamp > 30) {
        Block* block = blocks[last_created_block];
        if (block->get_prev_hash() == last_applied_block) {
            try_apply_block(block);
        } else {
            DEBUG_COUT("block->get_prev_hash != last_applied_block");
        }
    } else {
        DEBUG_COUT("last_applied_block == last_created_block");
    }

    return false;
}

bool ControllerImplementation::master()
{
    if (current_cores[0] == signer.get_mh_addr()) {
        return true;
    }

    return false;
}

bool ControllerImplementation::check_online_nodes()
{
    auto current_timestamp = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count());
    auto current_generation = current_timestamp / CORE_LIST_RENEW_PERIOD;

    auto online_cores = cores.get_online_cores();

    if (online_cores.size() < METAHASH_PRIMARY_CORES_COUNT) {
        return false;
    }

    if (core_list_generation < current_generation) {
        uint64_t accept_count = 0;
        if (current_generation - core_list_generation == 1) {
            accept_count = min_approve;
            if (std::find(current_cores.begin(), current_cores.end(), signer.get_mh_addr()) != std::end(current_cores)) {
                auto list = make_pretend_core_list();
                if (list.size()) {
                    cores.send_no_return(RPC_CORE_LIST_APPROVE, list);
                }

                std::string string_list;
                string_list.insert(string_list.end(), list.begin(), list.end());
            }
        } else {
            auto list = make_pretend_core_list();
            if (list.size()) {
                cores.send_no_return(RPC_CORE_LIST_APPROVE, list);
            }

            std::string string_list;
            string_list.insert(string_list.end(), list.begin(), list.end());

            {
                auto nodes = BC->get_node_state();
                for (auto&& [addr, roles] : nodes) {
                    if (roles.count(META_ROLE_CORE)) {
                        accept_count++;
                    }
                }
            }
            accept_count = std::ceil(accept_count * 51.0 / 100.0);
        }

        for (auto&& [cores_list, approve_cores] : proposed_cores[current_generation]) {

            if (approve_cores.size() >= accept_count) {
                auto primary_cores = crypto::split(cores_list, '\n');
                if (primary_cores.size() == METAHASH_PRIMARY_CORES_COUNT) {
                    current_cores = primary_cores;
                    core_list_generation = current_generation;

                    for (const auto& addr : current_cores) {
                        DEBUG_COUT(addr);
                    }

                    return true;
                }
            }
        }

        return false;
    }

    return true;
}

std::vector<char> ControllerImplementation::make_pretend_core_list()
{
    std::deque<std::string> cores_list;
    const auto nodes = BC->get_node_state();
    const auto online_cores = cores.get_online_cores();

//    for (auto&& [addr, roles] : nodes) {
//        if (roles.count(META_ROLE_CORE)) {
//            DEBUG_COUT("META_ROLE_CORE\t" + addr);
//        }
//    }
//    for (auto& addr : online_cores) {
//        if (online_cores.count(addr)) {
//            DEBUG_COUT("online_cores\t" + addr);
//        }
//    }

    for (auto&& [addr, roles] : nodes) {
        if (online_cores.count(addr) && roles.count(META_ROLE_CORE)) {
//            DEBUG_COUT("online_cores && META_ROLE_CORE\t" + addr);
            cores_list.push_back(addr);
        }
    }

    std::string master;
    std::set<std::string> slaves;
    if (cores_list.size() >= METAHASH_PRIMARY_CORES_COUNT) {
        uint64_t last_block_hash_xx64 = crypto::get_xxhash64(blocks[last_applied_block]->get_data());
        {
            std::sort(cores_list.begin(), cores_list.end());
//            DEBUG_COUT("last_block_hash_xx64\t" + std::to_string(last_block_hash_xx64));
            std::mt19937_64 r;
            r.seed(last_block_hash_xx64);
            std::shuffle(cores_list.begin(), cores_list.end(), r);
        }

        master = cores_list[0];
//        DEBUG_COUT("META_ROLE_MASTER\t" + master);
        for (uint i = 1; i < METAHASH_PRIMARY_CORES_COUNT; i++) {
            slaves.insert(cores_list[i]);
//            DEBUG_COUT("META_ROLE_SLAVE\t" + cores_list[i]);
        }
    }

    std::vector<char> return_list;
    return_list.insert(return_list.end(), master.begin(), master.end());
    for (const auto& addr : slaves) {
        return_list.push_back('\n');
        return_list.insert(return_list.end(), addr.begin(), addr.end());
    }

    return return_list;
}

}