#include "controller.hpp"
#include <meta_constants.hpp>
//#include <meta_log.hpp>

namespace metahash::meta_core {

void ControllerImplementation::approve_block(block::Block* p_block)
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

void ControllerImplementation::apply_approve(transaction::ApproveRecord* p_ar)
{
    sha256_2 block_hash;
    std::copy(p_ar->block_hash.begin(), p_ar->block_hash.end(), block_hash.begin());

    std::string addr = "0x" + crypto::bin2hex(crypto::get_address(p_ar->pub_key));

    if (p_ar->approve) {
        if (!block_approve[block_hash].insert({ addr, p_ar }).second) {
            delete p_ar;
        }
    } else {
        if (!block_disapprove[block_hash].insert({ addr, p_ar }).second) {
            delete p_ar;
        }
    }
}

bool ControllerImplementation::count_approve_for_block(block::Block* block)
{
    auto block_hash = block->get_block_hash();

    uint64_t approve_size = 0;
    for (auto&& [addr, _] : block_approve[block_hash]) {
        if (std::find(current_cores.begin(), current_cores.end(), addr) != std::end(current_cores)) {
            approve_size++;
        }
    }

    if (approve_size >= min_approve || approve_size == current_cores.size() || block->is_local()) {
        return try_apply_block(block);
    }

    get_approve_for_block(block_hash);
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

            core_last_block[signer.get_mh_addr()] = prev_timestamp;
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

                allowed_addresses.erase(signer.get_mh_addr());

                listener.update_allowed_addreses(allowed_addresses);
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

bool ControllerImplementation::check_block_for_appliance_and_break_on_corrupt_block(block::Block*& block)
{
    sha256_2 hash = block->get_block_hash();
    if (!block_approve[hash].count(signer.get_mh_addr()) && !block_disapprove[hash].count(signer.get_mh_addr())) {
        if (BC.can_apply_block(block)) {
            approve_block(block);

            return count_approve_for_block(block);
        } else {
            disapprove_block(block);

            blocks.erase(hash);

            return true;
        }
    } else {
        return count_approve_for_block(block);
    }

    return false;
}

}