#include <meta_chain.h>

#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_chain {

void BlockChain::make_forging_block_coin_reward(uint64_t timestamp, std::map<std::string, uint64_t>& delegates, std::vector<char>& txs_buff)
{
    auto state_fee = wallet_map.get_wallet(STATE_FEE_WALLET);

    uint64_t forging_coin_units = 0;
    for (auto& delegate_pair : delegates) {
        forging_coin_units += delegate_pair.second;
    }

    if (forging_coin_units) {
        uint64_t forging_coin_total = ((FORGING_POOL(timestamp) + state_fee->get_value()) * 5) / 10;
        double forging_coin_per_one = double(forging_coin_total) / double(forging_coin_units);

        for (auto& delegate_pair : delegates) {
            std::string coin_addres = delegate_pair.first;
            auto forging_coin = uint64_t(forging_coin_per_one * double(delegate_pair.second));

            if (forging_coin_total < forging_coin) {
                continue;
            }

            forging_coin_total -= forging_coin;

            auto&& state_tx = make_forging_tx(coin_addres, forging_coin, {}, TX_STATE_FORGING_C);

            if (!state_tx.empty()) {
                crypto::append_varint(txs_buff, state_tx.size());
                txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
            }
        }
    }
}

}