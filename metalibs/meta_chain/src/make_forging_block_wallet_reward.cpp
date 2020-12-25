#include <meta_chain.h>

#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_chain {

void BlockChain::make_forging_block_wallet_reward(uint64_t timestamp, std::map<std::string, uint64_t>& pasive_forging, std::vector<char>& txs_buff)
{
    auto state_fee = wallet_map.get_wallet(STATE_FEE_WALLET);

    if (!pasive_forging.empty()) {
        uint64_t forging_shares_total = 0;
        for (const auto& addr_pair : pasive_forging) {
            forging_shares_total += addr_pair.second;
        }

        const uint64_t forging_count_total = ((FORGING_POOL(timestamp) + state_fee->get_value()) * 1) / 10;

        const uint64_t FORGING_PASSIVE_REWARD = forging_count_total / 10;

        uint64_t reward_passive_per_share = FORGING_PASSIVE_REWARD / forging_shares_total;
        for (const auto& addr_pair : pasive_forging) {
            auto&& state_tx = make_forging_tx(addr_pair.first, addr_pair.second * reward_passive_per_share, {}, TX_STATE_FORGING_W);
            crypto::append_varint(txs_buff, state_tx.size());
            txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
        }
    }
}

}