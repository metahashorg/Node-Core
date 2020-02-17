#include "block.h"
#include <meta_log.hpp>
#include <open_ssl_decor.h>
#include <statics.hpp>

RejectedTXBlock::~RejectedTXBlock()
{
    for (auto& tx : txs) {
        delete tx;
    }
}

std::string RejectedTXBlock::get_sign()
{
    return std::string(sign);
}

std::string RejectedTXBlock::get_pub_key()
{
    return std::string(pub_key);
}

std::string RejectedTXBlock::get_data_for_sign()
{
    return std::string(data_for_sign);
}

const std::vector<RejectedTXInfo*>& RejectedTXBlock::get_txs()
{
    return txs;
}

bool RejectedTXBlock::parse(std::string_view block_sw)
{
    if (block_sw.size() < 80) {
        DEBUG_COUT("if (size < 80)");
        return false;
    }

    uint64_t sign_start = 0;
    uint64_t sign_size = 0;
    std::string_view tmp_sign;
    uint64_t pubk_start = 0;
    uint64_t pubk_size = 0;
    std::string_view tmp_pubk;
    uint64_t data_start = 0;
    uint64_t data_size = 0;
    std::string_view tmp_data_for_sign;

    uint64_t cur_pos = 0;

    block_type = *(reinterpret_cast<const uint64_t*>(&block_sw[cur_pos]));
    cur_pos += sizeof(uint64_t);

    block_timestamp = *(reinterpret_cast<const uint64_t*>(&block_sw[cur_pos]));
    cur_pos += sizeof(uint64_t);

    std::copy_n(block_sw.begin() + cur_pos, 32, prev_hash.begin());
    cur_pos += 32;

    {
        std::string_view varint_arr(&block_sw[cur_pos], block_sw.size() - cur_pos);
        uint64_t varint_size = read_varint(sign_size, varint_arr);
        if (varint_size < 1) {
            DEBUG_COUT("corrupt varint size");
            return false;
        }
        cur_pos += varint_size;
        sign_start = cur_pos;

        if (cur_pos + sign_size >= block_sw.size()) {
            DEBUG_COUT("corrupt sign size");
            return false;
        }
        tmp_sign = std::string_view(&block_sw[cur_pos], sign_size);
        cur_pos += sign_size;
    }

    {
        std::string_view varint_arr(&block_sw[cur_pos], block_sw.size() - cur_pos);
        uint64_t varint_size = read_varint(pubk_size, varint_arr);
        if (varint_size < 1) {
            DEBUG_COUT("corrupt varint size");
            return false;
        }
        cur_pos += varint_size;
        pubk_start = cur_pos;

        if (cur_pos + pubk_size >= block_sw.size()) {
            DEBUG_COUT("corrupt pubk size");
            return false;
        }
        tmp_pubk = std::string_view(&block_sw[cur_pos], pubk_size);
        cur_pos += pubk_size;
    }

    data_start = cur_pos;
    data_size = block_sw.size() - cur_pos;
    tmp_data_for_sign = std::string_view(&block_sw[data_start], data_size);

    if (!check_sign(tmp_data_for_sign, tmp_sign, tmp_pubk)) {
        DEBUG_COUT("Invalid Block Sign");
        return false;
    }

    txs.reserve(block_sw.size() / 32);

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
        auto* tx = new RejectedTXInfo;
        if (tx->parse(tx_as_sw)) {
            txs.push_back(tx);
        } else {
            DEBUG_COUT("TX PARSE ERROR");
            delete tx;
            return false;
        }
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
    data.insert(data.end(), block_sw.begin(), block_sw.end());

    sign = std::string_view(&data[sign_start], sign_size);
    pub_key = std::string_view(&data[pubk_start], pubk_size);
    data_for_sign = std::string_view(&data[data_start], data_size);

    block_hash = get_sha256(data);

    return true;
}

void RejectedTXBlock::clean()
{
    for (auto tx : txs) {
        delete tx;
    }
    txs.resize(0);
    txs.shrink_to_fit();
}

bool RejectedTXBlock::make(
    uint64_t timestamp,
    const sha256_2& new_prev_hash,
    const std::vector<RejectedTXInfo*>& new_txs,
    const std::vector<char>& PrivKey,
    const std::vector<char>& PubKey)
{
    std::vector<char> tx_buff;

    std::map<sha256_2, RejectedTXInfo*> tx_map;
    for (auto tx : new_txs) {
        tx_map.insert({ tx->tx_hash, tx });
    }

    {
        for (auto [hash, tx] : tx_map) {
            append_varint(tx_buff, tx->data.size());
            tx_buff.insert(tx_buff.end(), tx->data.begin(), tx->data.end());
        }
        append_varint(tx_buff, 0);
    }

    std::vector<char> sign_buff;
    sign_data(tx_buff, sign_buff, PrivKey);

    uint64_t b_type = BLOCK_TYPE_TECH_BAD_TX;
    std::vector<char> block_buff;

    block_buff.insert(block_buff.end(), reinterpret_cast<char*>(&b_type), (reinterpret_cast<char*>(&b_type) + sizeof(uint64_t)));
    block_buff.insert(block_buff.end(), reinterpret_cast<char*>(&timestamp), (reinterpret_cast<char*>(&timestamp) + sizeof(uint64_t)));
    block_buff.insert(block_buff.end(), new_prev_hash.begin(), new_prev_hash.end());
    append_varint(block_buff, sign_buff.size());
    block_buff.insert(block_buff.end(), sign_buff.begin(), sign_buff.end());
    append_varint(block_buff, PubKey.size());
    block_buff.insert(block_buff.end(), PubKey.begin(), PubKey.end());
    block_buff.insert(block_buff.end(), tx_buff.begin(), tx_buff.end());

    std::string_view block_as_sw(block_buff.data(), block_buff.size());

    return parse(block_as_sw);
}