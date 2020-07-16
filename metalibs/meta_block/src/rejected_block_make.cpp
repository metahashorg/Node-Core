#include <meta_block.h>
#include <meta_constants.hpp>
#include <meta_crypto.h>

namespace metahash::block {

bool RejectedTXBlock::make(
    uint64_t timestamp,
    const sha256_2& new_prev_hash,
    const std::vector<transaction::RejectedTXInfo*>& new_txs,
    crypto::Signer& signer)
{
    std::vector<char> tx_data_buff;

    std::map<sha256_2, transaction::RejectedTXInfo*> tx_map;
    for (auto tx : new_txs) {
        tx_map.insert({ tx->tx_hash, tx });
    }

    {
        for (auto [hash, tx] : tx_map) {
            crypto::append_varint(tx_data_buff, tx->data.size());
            tx_data_buff.insert(tx_data_buff.end(), tx->data.begin(), tx->data.end());
        }
        crypto::append_varint(tx_data_buff, 0);
    }

    std::vector<char> sign_buff = signer.sign(tx_data_buff);
    std::vector<char> PubKey = signer.get_pub_key();

    uint64_t b_type = BLOCK_TYPE_TECH_BAD_TX;
    std::vector<char> block_buff;

    block_buff.insert(block_buff.end(), reinterpret_cast<char*>(&b_type), (reinterpret_cast<char*>(&b_type) + sizeof(uint64_t)));
    block_buff.insert(block_buff.end(), reinterpret_cast<char*>(&timestamp), (reinterpret_cast<char*>(&timestamp) + sizeof(uint64_t)));
    block_buff.insert(block_buff.end(), new_prev_hash.begin(), new_prev_hash.end());
    crypto::append_varint(block_buff, sign_buff.size());
    block_buff.insert(block_buff.end(), sign_buff.begin(), sign_buff.end());
    crypto::append_varint(block_buff, PubKey.size());
    block_buff.insert(block_buff.end(), PubKey.begin(), PubKey.end());
    block_buff.insert(block_buff.end(), tx_data_buff.begin(), tx_data_buff.end());

    std::string_view block_as_sw(block_buff.data(), block_buff.size());

    return parse(block_as_sw);
}

}