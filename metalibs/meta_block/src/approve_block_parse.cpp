#include <meta_block.h>
#include <meta_crypto.h>
#include <meta_log.hpp>

namespace metahash::block {

bool ApproveBlock::parse(std::string_view block_sw)
{
    if (block_sw.size() < 48) {
        DEBUG_COUT("if (size < 80)");
        return false;
    }

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

        std::string_view tx_as_sw(&block_sw[cur_pos], tx_size);
        auto* tx = new transaction::ApproveRecord;
        if (!tx->parse(tx_as_sw)) {
            DEBUG_COUT("TX PARSE ERROR");
            delete tx;
            return false;
        }
        delete tx;

        cur_pos += tx_size;
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

    data.clear();
    data.insert(data.end(), block_sw.begin(), block_sw.begin() + cur_pos);

    return true;
}

}