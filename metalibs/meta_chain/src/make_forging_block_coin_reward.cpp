#include <meta_chain.h>

#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_chain {

void BlockChain::make_forging_block_coin_reward(const uint64_t pool, std::map<std::string, uint64_t>& delegates, std::vector<char>& txs_buff)
{
    uint64_t forging_coin_units = 0;
    for (auto& delegate_pair : delegates) {
        forging_coin_units += delegate_pair.second;
    }

    if (forging_coin_units) {
        uint64_t forging_coin_total = (pool * 50) / 100;
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