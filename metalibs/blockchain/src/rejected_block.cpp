#include "block.h"
#include <meta_log.hpp>
#include <open_ssl_decor.h>
#include <statics.hpp>

namespace metahash::metachain {

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

const std::vector<RejectedTXInfo> RejectedTXBlock::get_txs()
{
    std::vector<RejectedTXInfo> txs;
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

    uint64_t cur_pos = 48;

    {
        std::string_view varint_arr(&block_sw[cur_pos], block_sw.size() - cur_pos);
        uint64_t varint_size = crypto::read_varint(sign_size, varint_arr);
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
        uint64_t varint_size = crypto::read_varint(pubk_size, varint_arr);
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

    if (!crypto::check_sign(tmp_data_for_sign, tmp_sign, tmp_pubk)) {
        DEBUG_COUT("Invalid Block Sign");
        return false;
    }

    tx_buff = cur_pos;

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
        auto* tx = new RejectedTXInfo;
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
    data.insert(data.end(), block_sw.begin(), block_sw.end());

    sign = std::string_view(&data[sign_start], sign_size);
    pub_key = std::string_view(&data[pubk_start], pubk_size);
    data_for_sign = std::string_view(&data[data_start], data_size);

    return true;
}

bool RejectedTXBlock::make(
    uint64_t timestamp,
    const sha256_2& new_prev_hash,
    const std::vector<RejectedTXInfo*>& new_txs,
    crypto::Signer& signer)
{
    std::vector<char> tx_data_buff;

    std::map<sha256_2, RejectedTXInfo*> tx_map;
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