#include "block.h"
#include <meta_log.hpp>
#include <open_ssl_decor.h>
#include <statics.hpp>

const std::vector<TX> CommonBlock::get_txs()
{
    if (data.empty()) {
        return std::vector<TX>();
    }

    uint64_t cur_pos = tx_buff;
    uint64_t tx_size;
    {
        std::string_view tx_size_arr(&data[cur_pos], data.size() - cur_pos);
        uint64_t varint_size = read_varint(tx_size, tx_size_arr);
        cur_pos += varint_size;
    }

    bool SKIP_CHECK_SIGN = (get_block_type() == BLOCK_TYPE_STATE || get_block_type() == BLOCK_TYPE_FORGING || get_prev_hash() == sha256_2 {});

    std::vector<std::string_view> tx_buffs;
    while (tx_size > 0) {
        std::string_view tx_sw(&data[cur_pos], tx_size);
        cur_pos += tx_size;
        tx_buffs.push_back(tx_sw);

        {
            std::string_view tx_size_arr = std::string_view(&data[cur_pos], data.size() - cur_pos);
            uint64_t varint_size = read_varint(tx_size, tx_size_arr);
            cur_pos += varint_size;
        }
    }

    std::vector<TX> txs(tx_buffs.size());
    uint64_t i = 0;
    for (auto&& tx_data : tx_buffs) {
        txs[i].parse(tx_data, !SKIP_CHECK_SIGN);
        i++;
    }

    std::sort(txs.begin(), txs.end(), [](TX& lh, TX& rh) { return lh.nonce < rh.nonce; });

    return txs;
}

uint64_t CommonBlock::get_block_type()
{
    uint64_t block_type = Block::get_block_type();
    if (block_type == BLOCK_TYPE) {
        block_type = BLOCK_TYPE_COMMON;
    }
    return block_type;
}

bool CommonBlock::parse(std::string_view block_sw)
{
    sha256_2 tx_hash_calc = { { 0 } };

    if (block_sw.size() < 80) {
        DEBUG_COUT("if (size < 80)");
        return false;
    }

    uint64_t block_type = *(reinterpret_cast<const uint64_t*>(&block_sw[0]));

    sha256_2 prev_hash;
    std::copy_n(block_sw.begin() + 16, 32, prev_hash.begin());

    bool prev_is_zero = prev_hash == tx_hash_calc;

    sha256_2 tx_hash;
    std::copy_n(block_sw.begin() + 48, 32, tx_hash.begin());

    uint64_t cur_pos = tx_buff;
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

    while (tx_size > 0) {
        if (cur_pos + tx_size >= block_sw.size()) {
            DEBUG_COUT("TX BUFF ERROR");
            return false;
        }
        std::string_view tx_sw(block_sw.begin() + cur_pos, tx_size);
        cur_pos += tx_size;

        auto* tx = new TX;
        bool SKIP_CHECK_SIGN = (block_type == BLOCK_TYPE_STATE || block_type == BLOCK_TYPE_FORGING || prev_is_zero);
        if (!tx->parse(tx_sw, !SKIP_CHECK_SIGN)) {
            DEBUG_COUT("tx->parse");
            delete tx;
            return false;
        }
        delete tx;

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

    data.clear();
    data.insert(data.end(), block_sw.begin(), block_sw.begin() + cur_pos);

    return true;
}