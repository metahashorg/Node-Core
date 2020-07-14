#include <meta_block.h>
#include <meta_constants.hpp>
#include <meta_crypto.h>
#include <meta_log.hpp>

namespace metahash::block {

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
        uint64_t varint_size = crypto::read_varint(tx_size, tx_size_arr);
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

        auto* tx = new transaction::TX;
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
            uint64_t varint_size = crypto::read_varint(tx_size, tx_size_arr);
            if (varint_size < 1) {
                DEBUG_COUT("VARINT READ ERROR");
                return false;
            }
            cur_pos += varint_size;
        }
    }

    std::string_view txs_sw(block_sw.begin() + tx_buff, cur_pos - tx_buff);
    tx_hash_calc = crypto::get_sha256(txs_sw);
    if (tx_hash_calc != tx_hash) {
        DEBUG_COUT("tx_hash_calc != tx_hash");
        return false;
    }

    data.clear();
    data.insert(data.end(), block_sw.begin(), block_sw.begin() + cur_pos);

    return true;
}

}