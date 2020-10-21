#include "controller.hpp"
#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_core {

bool ControllerImplementation::try_make_block(uint64_t timestamp)
{
    auto lower_bound = core_list_generation * CORE_LIST_RENEW_PERIOD + CORE_LIST_SILENCE_PERIOD;
    auto upper_bound = (core_list_generation + 1) * CORE_LIST_RENEW_PERIOD - CORE_LIST_SILENCE_PERIOD;
    if (timestamp < lower_bound || timestamp > upper_bound) {
        return false;
    }

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
            block::Block* block = nullptr;
            switch (block_state) {
            case BLOCK_TYPE_COMMON:
                if (timestamp - statistics_timestamp > 600) {
                    block = BC.make_statistics_block(timestamp);
                    statistics_timestamp = timestamp;
                } else {
                    if (!transactions.empty()) {
                        block = BC.make_common_block(timestamp, transactions);
                    }
                }
                break;
            case BLOCK_TYPE_FORGING:
                DEBUG_COUT("BLOCK_TYPE_FORGING");
                block = BC.make_forging_block(timestamp);
                break;
            case BLOCK_TYPE_STATE:
                DEBUG_COUT("BLOCK_TYPE_STATE");
                block = BC.make_state_block(timestamp);
                break;
            }

            if (block) {
                if (BC.can_apply_block(block)) {
                    last_created_block = block->get_block_hash();
                    blocks.insert(block);
                    distribute(block);
                    approve_block(block);
                    return true;
                }

                DEBUG_COUT("FUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU");
                std::this_thread::sleep_for(std::chrono::seconds(3));
                exit(1);
            }
        }

        if (auto tx_list = BC.make_rejected_tx_block(timestamp)) {
            rejected_tx_list.insert(rejected_tx_list.end(), tx_list->begin(), tx_list->end());
            delete tx_list;

            auto reject_tx_block = new block::RejectedTXBlock;
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
    }

    return false;
}

}