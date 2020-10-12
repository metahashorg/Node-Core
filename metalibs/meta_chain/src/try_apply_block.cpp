#include <meta_chain.h>
#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_chain {

bool BlockChain::try_apply_block(block::Block* block, bool apply)
{
    static const sha256_2 zero_hash = { { 0 } };
    bool check_state = true;

    if (wallet_map.empty() && (block->get_block_type() == BLOCK_TYPE_STATE || block->get_prev_hash() == zero_hash)) {
        check_state = false;
        DEBUG_COUT("block is state or like\t" + crypto::bin2hex(block->get_block_hash()));
    } else if (block->get_prev_hash() != prev_hash) {
        DEBUG_COUT("prev hash not equal in block and database");
        return false;
    }

    bool status = false;
    switch (block->get_block_type()) {
    case BLOCK_TYPE_COMMON: {
        if (check_state) {
            status = can_apply_common_block(block);
        } else {
            status = can_apply_state_block(block, check_state);
        }
    } break;
    case BLOCK_TYPE_STATE: {
        status = can_apply_state_block(block, check_state);
    } break;
    case BLOCK_TYPE_FORGING: {
        status = can_apply_forging_block(block);
    } break;
    default:
        DEBUG_COUT("wrong block typo");
        return false;
    }

    if (status && apply) {
        wallet_map.apply_changes();

        return true;
    }

    wallet_map.clear_changes();
    return status;
}

}