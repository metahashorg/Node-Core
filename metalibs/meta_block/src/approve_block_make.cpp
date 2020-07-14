#include <meta_block.h>
#include <meta_constants.hpp>
#include <meta_crypto.h>

namespace metahash::block {

bool ApproveBlock::make(uint64_t timestamp, const sha256_2& new_prev_hash, const std::vector<transaction::ApproveRecord*>& new_txs)
{
    std::vector<char> block_buff;
    uint64_t b_type = BLOCK_TYPE_TECH_APPROVE;

    block_buff.insert(block_buff.end(), reinterpret_cast<char*>(&b_type), (reinterpret_cast<char*>(&b_type) + sizeof(uint64_t)));
    block_buff.insert(block_buff.end(), reinterpret_cast<char*>(&timestamp), (reinterpret_cast<char*>(&timestamp) + sizeof(uint64_t)));
    block_buff.insert(block_buff.end(), new_prev_hash.begin(), new_prev_hash.end());
    for (auto tx : new_txs) {
        crypto::append_varint(block_buff, tx->data.size());
        block_buff.insert(block_buff.end(), tx->data.begin(), tx->data.end());
    }
    crypto::append_varint(block_buff, 0);

    std::string_view block_as_sw(block_buff.data(), block_buff.size());

    return parse(block_as_sw);
}

}