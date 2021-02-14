#include <meta_block.h>
#include <meta_crypto.h>

namespace metahash::block {

const std::vector<transaction::ApproveRecord> ApproveBlock::get_txs() const
{
    std::vector<transaction::ApproveRecord> txs;
    if (data.empty()) {
        return txs;
    }

    uint64_t cur_pos = tx_buff;
    uint64_t tx_size = 0;
    {
        std::string_view tx_size_arr(&data[cur_pos], data.size() - cur_pos);
        uint64_t varint_size = crypto::read_varint(tx_size, tx_size_arr);
        cur_pos += varint_size;
    }

    std::vector<std::string_view> tx_buffs;
    while (tx_size > 0) {
        std::string_view tx_sw(&data[cur_pos], tx_size);
        cur_pos += tx_size;
        tx_buffs.push_back(tx_sw);

        {
            std::string_view tx_size_arr = std::string_view(&data[cur_pos], data.size() - cur_pos);
            uint64_t varint_size = crypto::read_varint(tx_size, tx_size_arr);
            cur_pos += varint_size;
        }
    }

    txs.resize(tx_buffs.size());
    uint64_t i = 0;
    for (auto&& tx_data : tx_buffs) {
        txs[i].parse(tx_data);
        i++;
    }

    return txs;
}

}