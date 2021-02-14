#include <meta_block.h>
#include <meta_constants.hpp>
#include <meta_crypto.h>

#include <future>
#include <list>

namespace metahash::block {

const std::vector<transaction::TX> CommonBlock::get_txs(boost::asio::io_context& io_context) const
{
    if (data.empty()) {
        return std::vector<transaction::TX>();
    }

    uint64_t cur_pos = tx_buff;
    uint64_t tx_size = 0;
    {
        std::string_view tx_size_arr(&data[cur_pos], data.size() - cur_pos);
        uint64_t varint_size = crypto::read_varint(tx_size, tx_size_arr);
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
            uint64_t varint_size = crypto::read_varint(tx_size, tx_size_arr);
            cur_pos += varint_size;
        }
    }

    std::list<std::future<bool>> pending_data;
    std::vector<transaction::TX> txs(tx_buffs.size());
    uint64_t i = 0;
    for (auto&& tx_data : tx_buffs) {
        auto promise = std::make_shared<std::promise<bool>>();
        pending_data.push_back(promise->get_future());

        io_context.post([&txs, promise, i, tx_data, SKIP_CHECK_SIGN] {
            promise->set_value(txs[i].parse(tx_data, !SKIP_CHECK_SIGN));
        });

        i++;
    }

    for (auto&& fut : pending_data) {
        fut.get();
    }

    std::sort(txs.begin(), txs.end(), [](transaction::TX& lh, transaction::TX& rh) { return lh.nonce < rh.nonce; });

    return txs;
}

uint64_t CommonBlock::get_block_type() const
{
    uint64_t block_type = Block::get_block_type();
    if (block_type == BLOCK_TYPE) {
        block_type = BLOCK_TYPE_COMMON;
    }
    return block_type;
}

}