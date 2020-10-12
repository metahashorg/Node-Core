#include "controller.hpp"
#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_core {

bool ControllerImplementation::approve_block(block::Block* p_block)
{
    auto* p_ar = new transaction::ApproveRecord;
    p_ar->make(p_block->get_block_hash(), signer);
    p_ar->approve = true;

    distribute(p_ar);
    return apply_approve(p_ar);
}

void ControllerImplementation::disapprove_block(block::Block* p_block)
{
    auto* p_ar = new transaction::ApproveRecord;
    p_ar->make(p_block->get_block_hash(), signer);
    p_ar->approve = false;

    distribute(p_ar);
    apply_approve(p_ar);
}

bool ControllerImplementation::apply_approve(transaction::ApproveRecord* p_ar)
{
    sha256_2 block_hash;
    std::copy(p_ar->block_hash.begin(), p_ar->block_hash.end(), block_hash.begin());

    std::string addr = "0x" + crypto::bin2hex(crypto::get_address(p_ar->pub_key));

    if (p_ar->approve) {
        {
            std::unique_lock lock(block_approve_lock);
            if (!block_approve[block_hash].insert({ addr, p_ar }).second) {
                delete p_ar;
            }
        }

        return count_approve_for_block(block_hash);
    } else {
        if (!block_disapprove[block_hash].insert({ addr, p_ar }).second) {
            delete p_ar;
        }
    }

    return false;
}

bool ControllerImplementation::count_approve_for_block(const sha256_2& block_hash)
{
    uint64_t approve_size = 0;
    for (auto&& [addr, _] : block_approve[block_hash]) {
        //DEBUG_COUT(addr);
        if (std::find(current_cores.begin(), current_cores.end(), addr) != std::end(current_cores)) {
            approve_size++;
        }
    }
    for (auto& addr : current_cores) {
        //DEBUG_COUT(addr);
    }

    //DEBUG_COUT(std::to_string(approve_size) + "\t" + std::to_string(min_approve) + "\t" + std::to_string(current_cores.size()));
    if (approve_size >= min_approve || approve_size == current_cores.size()) {
        if (await_blocks.count(block_hash)) {
            static const sha256_2 zero_block = { { 0 } };
            auto* block = await_blocks[block_hash];

            if (last_applied_block != zero_block) {
                if (block->get_prev_hash() == last_applied_block) {
                    return try_apply_block(block);
                }
            } else {
                if (proved_block != zero_block && block_hash == proved_block) {
                    return try_apply_block(block);
                } else if (proved_block == zero_block && block->get_prev_hash() == proved_block) {
                    return try_apply_block(block);
                }
            }
        }
    }

    return false;
}

bool ControllerImplementation::try_apply_block(block::Block* block, bool write)
{
    if (BC.apply_block(block)) {
        {
            prev_timestamp = block->get_block_timestamp();
            last_applied_block = block->get_block_hash();
            last_created_block = block->get_block_hash();

            prev_day = prev_timestamp / DAY_IN_SECONDS;
            prev_state = block->get_block_type();
        }

        {
            auto hash = block->get_block_hash();

            std::unique_lock ulock(blocks_lock);
            aplied_blocks.insert({ hash, block });
            await_blocks.erase(hash);
        }

        if (write) {
            write_block(block);

            if (block->get_block_type() == BLOCK_TYPE_STATE) {
                std::unordered_set<std::string, crypto::Hasher> allowed_addresses;
                for (auto&& [addr, roles] : BC.get_node_state()) {
                    if (roles.count(META_ROLE_CORE) || roles.count(META_ROLE_VERIF)) {
                        allowed_addresses.insert(addr);
                    }
                }
                listener->update_allowed_addreses(allowed_addresses);
            }
        }
        return true;
    }

    return false;
}

void ControllerImplementation::distribute(block::Block* block)
{
    std::vector<char> send_pack;
    send_pack.insert(send_pack.end(), block->get_data().begin(), block->get_data().end());

    cores.send_no_return(RPC_PRETEND_BLOCK, send_pack);
}

void ControllerImplementation::distribute(transaction::ApproveRecord* p_ar)
{
    auto approve_str = p_ar->approve ? RPC_APPROVE : RPC_DISAPPROVE;
    std::vector<char> send_pack;
    send_pack.insert(send_pack.end(), p_ar->data.begin(), p_ar->data.end());

    cores.send_no_return(approve_str, send_pack);
}

bool ControllerImplementation::master()
{
    if (!current_cores.empty() && current_cores[0] == signer.get_mh_addr()) {
        return true;
    }

    return false;
}

bool ControllerImplementation::check_block_for_appliance_and_break_on_corrupt_block(const sha256_2& hash, block::Block*& block)
{
    const bool return_break = true;

    if (!block_approve[hash].count(signer.get_mh_addr()) && !block_disapprove[hash].count(signer.get_mh_addr())) {
        if (BC.can_apply_block(block)) {
            return approve_block(block);
        } else {
            disapprove_block(block);

            std::unique_lock lock(blocks_lock);
            await_blocks.erase(hash);
            delete block;

            return return_break;
        }
    } else {
        return count_approve_for_block(hash);
    }

    return !return_break;
}

}