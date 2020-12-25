#include <meta_chain.h>

#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_chain {

uint64_t BlockChain::FORGING_POOL(uint64_t ts)
{
    uint64_t return_pool = 0;
    for (const auto [start_ts, pool] : FORGING_POOL_PER_YEAR) {
        if (ts > start_ts) {
            return_pool = pool;
        } else {
            break;
        }
    }
    return return_pool;
}

std::vector<char> BlockChain::make_forging_tx(const std::string& address, uint64_t reward, const std::vector<unsigned char>& data, uint64_t tx_type)
{
    std::vector<char> forging_tx;
    auto bin_address = crypto::hex2bin(address);

    forging_tx.insert(forging_tx.end(), bin_address.begin(), bin_address.end());

    crypto::append_varint(forging_tx, reward);
    crypto::append_varint(forging_tx, 0);
    crypto::append_varint(forging_tx, 0);

    crypto::append_varint(forging_tx, data.size());
    forging_tx.insert(forging_tx.end(), data.begin(), data.end());

    crypto::append_varint(forging_tx, 0);
    crypto::append_varint(forging_tx, 0);

    crypto::append_varint(forging_tx, tx_type);

    return forging_tx;
}

block::Block* BlockChain::make_forging_block(uint64_t timestamp)
{
    uint64_t block_type = BLOCK_TYPE_FORGING;
    std::vector<char> txs_buff;

    {
        auto&& [type_geo_node_delegates, delegates] = make_forging_block_get_node_stats();        

        make_forging_block_node_reward(timestamp, type_geo_node_delegates, txs_buff);
        make_forging_block_coin_reward(timestamp, delegates, txs_buff);
    }

    {
        auto&& [active_forging, pasive_forging] = make_forging_block_get_wallet_stats();

        make_forging_block_wallet_reward(timestamp, pasive_forging, txs_buff);
        make_forging_block_random_reward(timestamp, active_forging, txs_buff);
    }

    {
        auto&& state_tx = make_forging_tx(TEAM_WALLET, FORGING_TEAM_REWARD, {}, TX_STATE_FORGING_TEAM);

        crypto::append_varint(txs_buff, state_tx.size());
        txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
    }

    txs_buff.push_back(0);

    return make_block(block_type, timestamp, prev_hash, txs_buff);
}

}