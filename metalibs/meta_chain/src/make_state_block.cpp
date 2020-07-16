#include <meta_chain.h>
#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_chain {

block::Block* BlockChain::make_state_block(uint64_t timestamp)
{
    const uint64_t block_type = BLOCK_TYPE_STATE;
    std::vector<char> txs_buff;
    static const std::vector<unsigned char> zero_bin_addres(25, 0x00);

    for (auto& wallet_pair : wallet_map) {

        if (!wallet_pair.second) {
            DEBUG_COUT("invalid wallet:\t" + wallet_pair.first);
            continue;
        }

        std::vector<char> state_tx;
        auto bin_addres = crypto::hex2bin(wallet_pair.first);

        if (bin_addres.size() != 25) {
            continue;
        }

        if (bin_addres == zero_bin_addres) {
            wallet_pair.second->initialize(0, 0, "");
        }

        auto&& [value, nonce, json] = wallet_pair.second->serialize();

        state_tx.insert(state_tx.end(), bin_addres.begin(), bin_addres.end());

        crypto::append_varint(state_tx, value);
        crypto::append_varint(state_tx, 0);
        crypto::append_varint(state_tx, nonce);

        crypto::append_varint(state_tx, json.size());
        state_tx.insert(state_tx.end(), json.begin(), json.end());

        crypto::append_varint(state_tx, 0);
        crypto::append_varint(state_tx, 0);

        crypto::append_varint(state_tx, TX_STATE_STATE);

        crypto::append_varint(txs_buff, state_tx.size());
        txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
    }

    txs_buff.push_back(0);

    return make_block(block_type, timestamp, prev_hash, txs_buff);
}

}