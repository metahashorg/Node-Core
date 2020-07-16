#include <meta_constants.hpp>
#include <meta_log.hpp>

#include "controller.hpp"

#include <list>

namespace metahash::meta_core {

void ControllerImplementation::apply_block_chain(
    std::unordered_map<sha256_2, block::Block*, crypto::Hasher>& block_tree,
    std::unordered_map<sha256_2, block::Block*, crypto::Hasher>& prev_tree,
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

    std::list<block::Block*> block_chain;

    if (last_applied_block == zero_block && proved_block != zero_block) {
        sha256_2 curr_block = proved_block;

        bool got_start = false;
        while (block_tree.count(curr_block)) {
            auto* block = block_tree[curr_block];
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
            auto* block = prev_tree[curr_block];
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
                auto* p_ar = new transaction::ApproveRecord;
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

}