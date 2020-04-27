#include "block.h"
#include <meta_log.hpp>
#include <open_ssl_decor.h>
#include <statics.hpp>

const std::vector<ApproveRecord> ApproveBlock::get_txs()
{
    std::vector<ApproveRecord> txs;
    if (data.empty()) {
        return txs;
    }

    uint64_t cur_pos = tx_buff;
    uint64_t tx_size;
    {
        std::string_view tx_size_arr(&data[cur_pos], data.size() - cur_pos);
        uint64_t varint_size = read_varint(tx_size, tx_size_arr);
        cur_pos += varint_size;
    }

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

    txs.resize(tx_buffs.size());
    uint64_t i = 0;
    for (auto&& tx_data : tx_buffs) {
        txs[i].parse(tx_data);
        i++;
    }

    return txs;
}

bool ApproveBlock::parse(std::string_view block_sw)
{
    if (block_sw.size() < 80) {
        DEBUG_COUT("if (size < 80)");
        return false;
    }

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

        std::string_view tx_as_sw(&block_sw[cur_pos], tx_size);
        auto* tx = new ApproveRecord;
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
            uint64_t varint_size = read_varint(tx_size, tx_size_arr);
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

bool ApproveBlock::make(uint64_t timestamp, const sha256_2& new_prev_hash, const std::vector<ApproveRecord*>& new_txs)
{
    std::vector<char> block_buff;
    uint64_t b_type = BLOCK_TYPE_TECH_APPROVE;

    block_buff.insert(block_buff.end(), reinterpret_cast<char*>(&b_type), (reinterpret_cast<char*>(&b_type) + sizeof(uint64_t)));
    block_buff.insert(block_buff.end(), reinterpret_cast<char*>(&timestamp), (reinterpret_cast<char*>(&timestamp) + sizeof(uint64_t)));
    block_buff.insert(block_buff.end(), new_prev_hash.begin(), new_prev_hash.end());
    for (auto tx : new_txs) {
        append_varint(block_buff, tx->data.size());
        block_buff.insert(block_buff.end(), tx->data.begin(), tx->data.end());
    }
    append_varint(block_buff, 0);

    std::string_view block_as_sw(block_buff.data(), block_buff.size());

    return parse(block_as_sw);
}