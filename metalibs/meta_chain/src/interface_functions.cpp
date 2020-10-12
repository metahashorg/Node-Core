#include <meta_chain.h>
#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_chain {

bool BlockChain::can_apply_block(block::Block* block)
{
    return try_apply_block(block, false);
}

bool BlockChain::apply_block(block::Block* block)
{
    if (try_apply_block(block, true)) {
        clear = false;
        prev_hash = block->get_block_hash();
        if (block->get_block_type() == BLOCK_TYPE_STATE) {
            state_hash_xx64 = crypto::get_xxhash64(block->get_data());
            {
#include <ctime>
                std::time_t now = block->get_block_timestamp();
                std::tm* ptm = std::localtime(&now);
                char buffer[32] = { 0 };
                // Format: Mo, 20.02.2002 20:20:02
                std::strftime(buffer, 32, "%a, %d.%m.%Y %H:%M:%S", ptm);

                DEBUG_COUT(buffer);
            }
            fill_node_state();
        }

        return true;
    }

    wallet_map.clear_changes();
    return false;
}

std::vector<transaction::RejectedTXInfo*>* BlockChain::make_rejected_tx_block(uint64_t)
{
    if (rejected_tx_list.empty()) {
        return nullptr;
    } else {
        auto new_list = new std::vector<transaction::RejectedTXInfo*>(rejected_tx_list);
        rejected_tx_list.clear();
        return new_list;
    }
}

std::atomic<std::map<std::string, std::pair<uint, uint>>*>& BlockChain::get_wallet_statistics()
{
    return wallet_statistics;
}

std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& BlockChain::get_wallet_request_addresses()
{
    return wallet_request_addreses;
}

std::set<std::string> BlockChain::check_addr(const std::string& addr)
{
    if (node_state.find(addr) == node_state.end()) {
        return std::set<std::string>();
    } else {
        return node_state[addr];
    }
}

const std::unordered_map<std::string, std::set<std::string>, crypto::Hasher>& BlockChain::get_node_state()
{
    return node_state;
}

}