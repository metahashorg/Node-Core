#include "controller.hpp"
#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_core {

void ControllerImplementation::approve_block(block::Block* p_block)
{
    auto* p_ar = new transaction::ApproveRecord;
    p_ar->make(p_block->get_block_hash(), signer);
    p_ar->approve = true;

    distribute(p_ar);
    apply_approve(p_ar);
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
                auto* block = blocks[block_hash];
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

void ControllerImplementation::try_apply_block(block::Block* block)
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
    if (current_cores[0] == signer.get_mh_addr()) {
        return true;
    }

    return false;
}

}