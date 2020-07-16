#include <meta_chain.h>
#include <meta_constants.hpp>

namespace metahash::meta_chain {

block::Block* BlockChain::make_statistics_block(uint64_t timestamp)
{
    if (!statistics_tx_list.empty()) {
        uint64_t block_type = BLOCK_TYPE_COMMON;
        std::vector<char> txs_buff;

        std::sort(statistics_tx_list.begin(), statistics_tx_list.end(), [](transaction::TX* lh, transaction::TX* rh) {
            return lh->nonce < rh->nonce;
        });

        const uint64_t state = TX_STATE_TECH_NODE_STAT;
        const std::vector<unsigned char> state_as_varint_array = crypto::int_as_varint_array(state);
        for (transaction::TX* tx : statistics_tx_list) {
            crypto::append_varint(txs_buff, tx->raw_tx.size() + state_as_varint_array.size());
            txs_buff.insert(txs_buff.end(), tx->raw_tx.begin(), tx->raw_tx.end());
            txs_buff.insert(txs_buff.end(), state_as_varint_array.begin(), state_as_varint_array.end());

            delete tx;
        }
        statistics_tx_list.clear();
        txs_buff.push_back(0);

        return make_block(block_type, timestamp, prev_hash, txs_buff);
    }

    return nullptr;
}

}