#include "block.h"
#include <meta_log.hpp>
#include <open_ssl_decor.h>
#include <statics.hpp>

CommonBlock::~CommonBlock()
{
    for (auto& tx : txs) {
        delete tx;
    }
}

sha256_2 CommonBlock::get_prev_hash()
{
    return prev_hash;
}

const std::vector<TX*>& CommonBlock::get_txs()
{
    return txs;
}

bool CommonBlock::parse(std::string_view block_sw)
{
    std::array<unsigned char, 32> tx_hash_calc = { { 0 } };

    if (block_sw.size() < 80) {
        DEBUG_COUT("if (size < 80)");
        return false;
    }

    uint64_t cur_pos = 0;

    block_type = *(reinterpret_cast<const uint64_t*>(&block_sw[cur_pos]));
    cur_pos += sizeof(uint64_t);

    block_timestamp = *(reinterpret_cast<const uint64_t*>(&block_sw[cur_pos]));
    cur_pos += sizeof(uint64_t);

    std::copy_n(block_sw.begin() + cur_pos, 32, prev_hash.begin());
    cur_pos += 32;

    bool prev_is_zero = prev_hash == tx_hash_calc;

    std::copy_n(block_sw.begin() + cur_pos, 32, tx_hash.begin());
    cur_pos += 32;

    uint64_t tx_buff = cur_pos;

    uint64_t tx_size;
    {
        std::string_view tx_size_arr(
            block_sw.begin() + cur_pos,
            block_sw.size() - cur_pos);
        uint64_t varint_size = read_varint(tx_size, tx_size_arr);
        if (varint_size < 1) {
            DEBUG_COUT("VARINT READ ERROR");
            return false;
        }
        cur_pos += varint_size;
    }

    txs.reserve(block_sw.size() / 100);

    while (tx_size > 0) {
        if (cur_pos + tx_size >= block_sw.size()) {
            DEBUG_COUT("TX BUFF ERROR");
            return false;
        }
        std::string_view tx_sw(block_sw.begin() + cur_pos, tx_size);
        cur_pos += tx_size;

        auto* tx = new TX;
        bool SKIP_CHECK_SIGN = (block_type == BLOCK_TYPE_STATE || block_type == BLOCK_TYPE_FORGING || prev_is_zero);
        if (tx->parse(tx_sw, !SKIP_CHECK_SIGN)) {
            txs.push_back(tx);
        } else {
            DEBUG_COUT("tx->parse");
            delete tx;
            return false;
        }

        {
            std::string_view tx_size_arr = std::string_view(
                block_sw.begin() + cur_pos,
                block_sw.size() - cur_pos);
            uint64_t varint_size = read_varint(tx_size, tx_size_arr);
            if (varint_size < 1) {
                DEBUG_COUT("VARINT READ ERROR");
                return false;
            }
            cur_pos += varint_size;
        }
    }

    std::string_view txs_sw(block_sw.begin() + tx_buff, cur_pos - tx_buff);
    tx_hash_calc = get_sha256(txs_sw);
    if (tx_hash_calc != tx_hash) {
        DEBUG_COUT("tx_hash_calc != tx_hash");
        return false;
    }

    data.insert(data.end(), block_sw.begin(), block_sw.begin() + cur_pos);

    block_hash = get_sha256(data);

    if (block_type == BLOCK_TYPE) {
        block_type = BLOCK_TYPE_COMMON;
        std::sort(txs.begin(), txs.end(),
            [](TX* lh, TX* rh) { return lh->nonce < rh->nonce; });
    }

    return true;
}

void CommonBlock::clean()
{
    for (auto tx : txs) {
        delete tx;
    }
    txs.resize(0);
    txs.shrink_to_fit();
}