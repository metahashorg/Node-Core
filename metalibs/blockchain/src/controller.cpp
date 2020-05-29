#include "controller.hpp"

#include <blockchain.h>
#include <meta_log.hpp>
#include <open_ssl_decor.h>
#include <statics.hpp>

#include "block.h"
#include "chain.h"

#include <fstream>
#include <future>
#include <list>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

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
    , path(path)
    , signer(crypto::hex2bin(priv_key_line))
    , cores(io_context, host_port.first, host_port.second, core_list, signer)
{
    //    if (signer.get_mh_addr() == "0x00fca67778165988703a302c1dfc34fd6036e209a20666969e") {
    //        DEBUG_COUT("master");
    //        master = true;
    //    }

    {
        std::vector<unsigned char> bin_proved_hash = crypto::hex2bin(proved_hash);
        std::copy_n(bin_proved_hash.begin(), 32, proved_block.begin());
    }

    read_and_apply_local_chain();

    start_main_loop();

    listener = new net_io::meta_server(io_context, host_port.first, host_port.second, signer, [this](net_io::Request& request) -> std::vector<char> {
        return add_pack_to_queue(request);
    });
}

void ControllerImplementation::apply_block_chain(std::unordered_map<sha256_2, Block*, crypto::Hasher>& block_tree,
    std::unordered_map<sha256_2, Block*, crypto::Hasher>& prev_tree,
    const std::string& source, bool need_write)
{
    static const sha256_2 zero_block = { { 0 } };

    bool do_clean_and_return = false;
    if (last_applied_block == zero_block) {
        if (proved_block == zero_block) {
            if (prev_tree.find(zero_block) == prev_tree.end()) {
                DEBUG_COUT("blockchain from " + source + " have no blocks to connect with my chain to zero_block\t" + crypto::bin2hex(zero_block));
                do_clean_and_return = true;
            }
        } else if (block_tree.find(proved_block) == block_tree.end()) {
            DEBUG_COUT("blockchain from " + source + " have no blocks to connect with my chain to proved_block\t" + crypto::bin2hex(proved_block));
            do_clean_and_return = true;
        }
    } else if (prev_tree.find(last_applied_block) == prev_tree.end()) {
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
        while (block_tree.find(curr_block) != block_tree.end()) {
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

        while (prev_tree.find(curr_block) != prev_tree.end()) {
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
                if (blocks.find(hash) == blocks.end()) {
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

void ControllerImplementation::actualize_chain()
{
    DEBUG_COUT("actualize_chain");

    if (master()) {
        return;
    }

    std::map<std::string, std::vector<char>> last_block_on_cores = cores.send_with_return(RPC_LAST_BLOCK, std::vector<char>());

    std::set<std::string> cores_with_missing_block;
    std::set<sha256_2> missing_blocks;
    for (auto&& [core_addr, block_info] : last_block_on_cores) {
        if (block_info.size() >= 40) {
            sha256_2 last_block_return;
            std::copy_n(block_info.begin(), 32, last_block_return.begin());

            uint64_t block_timestamp = 0;
            std::copy_n(block_info.begin() + 32, 8, reinterpret_cast<char*>(&block_timestamp));

            if (block_timestamp > prev_timestamp && blocks.find(last_block_return) == blocks.end()) {
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
                        } else if (blocks.find(block->get_prev_hash()) == blocks.end()) {
                            if (block->get_prev_hash() == sha256_2 {}) {
                                DEBUG_COUT("Got complete chain. Previous block is zero block");
                            } else {
                                missing_blocks.insert(block->get_prev_hash());
                            }
                        }
                    } else {
                        failed ++;
                        delete block;
                        block = nullptr;
                    }
                } else {
                    failed ++;
                    DEBUG_COUT("Parse block error");
                }
            }
        }
    }
}

void ControllerImplementation::start_main_loop()
{
    std::thread(&ControllerImplementation::main_loop, this).detach();
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

std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& ControllerImplementation::get_wallet_request_addreses()
{
    return BC->get_wallet_request_addreses();
}

void ControllerImplementation::main_loop()
{
    std::this_thread::sleep_for(std::chrono::seconds(15));
    while (goon) {
        const uint64_t get_arr_size = 128;
        bool need_actualize = false;

        bool no_sleep = false;

        if (transactions.size() < 1024) {
            static TX* tx_arr[get_arr_size];
            uint64_t got_tx = tx_queue.try_dequeue_bulk(tx_arr, get_arr_size);

            if (got_tx) {
                no_sleep = true;
                std::copy(&tx_arr[0], &tx_arr[got_tx], std::back_inserter(transactions));
            }
        }

        {
            static Block* block_arr[get_arr_size];
            uint64_t got_block = block_queue.try_dequeue_bulk(block_arr, get_arr_size);

            if (got_block) {
                no_sleep = true;

                for (uint i = 0; i < got_block; i++) {
                    Block* block = block_arr[i];

                    if (dynamic_cast<CommonBlock*>(block)) {
                        if (blocks.find(block->get_prev_hash()) == blocks.end()) {
                            need_actualize = true;
                        }

                        if (blocks.insert({ block->get_block_hash(), block }).second) {
                            ;
                        } else {
                            delete block;
                        }
                    } else if (dynamic_cast<RejectedTXBlock*>(block)) {
                        write_block(block);
                    }
                }
            }

            for (std::pair<sha256_2, Block*> block_pair : blocks) {
                if (block_approve[block_pair.first].find(signer.get_mh_addr()) == block_approve[block_pair.first].end()
                    && block_disapprove[block_pair.first].find(signer.get_mh_addr()) == block_disapprove[block_pair.first].end()) {

                    Block* block = block_pair.second;
                    if (block->get_prev_hash() == last_applied_block) {
                        if (BC->can_apply_block(block)) {
                            approve_block(block);
                        } else {
                            disapprove_block(block);
                        }
                    }
                }
            }
        }

        {
            static ApproveRecord* approve_arr[get_arr_size];
            uint64_t got_approve = approve_queue.try_dequeue_bulk(approve_arr, get_arr_size);

            if (got_approve) {
                no_sleep = true;

                for (uint i = 0; i < got_approve; i++) {
                    apply_approve(approve_arr[i]);
                }
            }
        }

        if (master() && try_make_block()) {
            no_sleep = true;
        }

        {
            uint64_t timestamp = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count());

            if (timestamp - last_sync_timestamp > 60) {
                DEBUG_COUT("sync_core_lists");
                io_context.post([this] { cores.sync_core_lists(); });

                last_sync_timestamp = timestamp;
            }

            if (prev_timestamp > last_actualization_timestamp) {
                last_actualization_timestamp = prev_timestamp;
            }

            if (timestamp - last_actualization_timestamp > 1) {
                need_actualize = true;
            }

            if (need_actualize) {
                no_sleep = true;
                actualize_chain();
                need_actualize = false;
                last_actualization_timestamp = timestamp;
            }
        }

        if (no_sleep) {
            ;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
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

        auto approve_size = block_approve[block_hash].size();
        auto m_approve = min_approve();
        DEBUG_COUT(std::to_string(approve_size) + "\t" + std::to_string(m_approve));
        if (approve_size >= m_approve) {

            if (blocks.find(block_hash) != blocks.end()) {
                Block* block = blocks[block_hash];
                if (block->get_prev_hash() == last_applied_block) {
                    apply_block(block);
                } else {
                    DEBUG_COUT("block->get_prev_hash != last_applied_block");
                }
            } else {
                DEBUG_COUT("blocks.find(block_hash) == blocks.end()");
            }
        }
    } else {
        if (!block_disapprove[block_hash].insert({ addr, p_ar }).second) {
            delete p_ar;
        }
    }
}

void ControllerImplementation::apply_block(Block* block)
{
    if (BC->apply_block(block)) {
        write_block(block);

        if (block->get_block_type() == BLOCK_TYPE_STATE) {
            listener->update_allowed_addreses();
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

uint64_t ControllerImplementation::min_approve()
{
    uint64_t min_approve = 0;

    sha256_2 curr_block = blocks[last_applied_block]->get_prev_hash();
    uint i = 0;

    while (blocks.find(curr_block) != blocks.end() && i < 5) {
        i++;

        uint64_t curr_block_approve = block_approve[curr_block].size() + block_disapprove[curr_block].size();

        if (min_approve) {
            if (min_approve > curr_block_approve) {
                min_approve = curr_block_approve;
            }
        } else {
            min_approve = curr_block_approve;
        }

        curr_block = blocks[curr_block]->get_prev_hash();
    }

    min_approve = min_approve * 51 / 100;

    if (min_approve > 1) {
        return min_approve;
    } else {
        if (master()) {
            return 1;
        } else {
            return 2;
        }
    }
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
            apply_block(block);
        } else {
            DEBUG_COUT("block->get_prev_hash != last_applied_block");
        }
    }
    return false;
}

bool ControllerImplementation::master()
{
    uint64_t timestamp = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count());
    uint64_t current_day = timestamp / DAY_IN_SECONDS;
    if (current_day != prev_day) {
        return false;
    }

    BC->check_addr(signer.get_mh_addr()) == MASTER_CORE_ROLE;

    return false;
}

}