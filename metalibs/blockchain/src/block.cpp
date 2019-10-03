#include <block.h>
#include <meta_log.hpp>
#include <open_ssl_decor.h>
#include <statics.hpp>

Block* parse_block(std::string_view block_sw)
{
    uint64_t block_type = 0;

    if (block_sw.size() < 8) {
        DEBUG_COUT("if (size < 8)");
        return nullptr;
    }

    block_type = *(reinterpret_cast<const uint64_t*>(&block_sw[0]));

    switch (block_type) {
    case BLOCK_TYPE:
    case BLOCK_TYPE_COMMON:
    case BLOCK_TYPE_STATE:
    case BLOCK_TYPE_FORGING:
        return (new CommonBlock())->parse(block_sw);
        break;
    case BLOCK_TYPE_TECH_BAD_TX:
        return (new RejectedTXBlock())->parse(block_sw);
        break;
    case BLOCK_TYPE_TECH_APPROVE:
        return (new ApproveBlock())->parse(block_sw);
    default:
        return nullptr;
    }
}

const std::vector<char>& Block::get_data()
{
    return data;
}

uint64_t Block::get_block_type()
{
    return block_type;
}

uint64_t Block::get_block_timestamp()
{
    return block_timestamp;
}

sha256_2 Block::get_prev_hash()
{
    return prev_hash;
}

sha256_2 Block::get_block_hash()
{
    return block_hash;
}

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

Block* CommonBlock::parse(std::string_view block_sw)
{
    std::array<unsigned char, 32> tx_hash_calc = { { 0 } };

    if (block_sw.size() < 80) {
        DEBUG_COUT("if (size < 80)");
        delete this;
        return nullptr;
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
            delete this;
            return nullptr;
        }
        cur_pos += varint_size;
    }

    txs.reserve(block_sw.size() / 100);

    while (tx_size > 0) {
        if (cur_pos + tx_size >= block_sw.size()) {
            DEBUG_COUT("TX BUFF ERROR");
            delete this;
            return nullptr;
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
            delete this;
            return nullptr;
        }

        {
            std::string_view tx_size_arr = std::string_view(
                block_sw.begin() + cur_pos,
                block_sw.size() - cur_pos);
            uint64_t varint_size = read_varint(tx_size, tx_size_arr);
            if (varint_size < 1) {
                DEBUG_COUT("VARINT READ ERROR");
                delete this;
                return nullptr;
            }
            cur_pos += varint_size;
        }
    }

    std::string_view txs_sw(block_sw.begin() + tx_buff, cur_pos - tx_buff);
    tx_hash_calc = get_sha256(txs_sw);
    if (tx_hash_calc != tx_hash) {
        DEBUG_COUT("tx_hash_calc != tx_hash");
        delete this;
        return nullptr;
    }

    data.insert(data.end(), block_sw.begin(), block_sw.begin() + cur_pos);

    block_hash = get_sha256(data);

    if (block_type == BLOCK_TYPE) {
        block_type = BLOCK_TYPE_COMMON;
        std::sort(txs.begin(), txs.end(),
            [](TX* lh, TX* rh) { return lh->nonce < rh->nonce; });
    }

    return this;
}

void CommonBlock::clean()
{
    for (auto tx : txs) {
        delete tx;
    }
    txs.resize(0);
    txs.shrink_to_fit();
}

ApproveBlock::~ApproveBlock()
{
    for (auto& tx : txs) {
        delete tx;
    }
}

const std::vector<ApproveRecord*>& ApproveBlock::get_txs()
{
    return txs;
}

Block* ApproveBlock::parse(std::string_view block_sw)
{
    if (block_sw.size() < 80) {
        DEBUG_COUT("if (size < 80)");
        delete this;
        return nullptr;
    }

    uint64_t cur_pos = 0;

    block_type = *(reinterpret_cast<const uint64_t*>(&block_sw[cur_pos]));
    cur_pos += sizeof(uint64_t);

    block_timestamp = *(reinterpret_cast<const uint64_t*>(&block_sw[cur_pos]));
    cur_pos += sizeof(uint64_t);

    std::copy_n(block_sw.begin() + cur_pos, 32, prev_hash.begin());
    cur_pos += 32;

    txs.reserve(block_sw.size() / 128);

    uint64_t tx_size;
    {
        std::string_view tx_size_arr(
            block_sw.begin() + cur_pos,
            block_sw.size() - cur_pos);
        uint64_t varint_size = read_varint(tx_size, tx_size_arr);
        if (varint_size < 1) {
            DEBUG_COUT("VARINT READ ERROR");
            delete this;
            return nullptr;
        }
        cur_pos += varint_size;
    }

    while (tx_size > 0) {
        if (cur_pos + tx_size >= block_sw.size()) {
            DEBUG_COUT("TX BUFF ERROR");
            delete this;
            return nullptr;
        }

        std::string_view tx_as_sw(&block_sw[cur_pos], tx_size);
        auto* tx = new ApproveRecord;
        if (tx->parse(tx_as_sw)) {
            txs.push_back(tx);
        } else {
            DEBUG_COUT("TX PARSE ERROR");
            delete tx;
            delete this;
            return nullptr;
        }
        cur_pos += tx_size;
        {
            std::string_view tx_size_arr = std::string_view(
                block_sw.begin() + cur_pos,
                block_sw.size() - cur_pos);
            uint64_t varint_size = read_varint(tx_size, tx_size_arr);
            if (varint_size < 1) {
                DEBUG_COUT("VARINT READ ERROR");
                delete this;
                return nullptr;
            }
            cur_pos += varint_size;
        }
    }

    data.insert(data.end(), block_sw.begin(), block_sw.begin() + cur_pos);

    block_hash = get_sha256(data);

    return this;
}

void ApproveBlock::clean()
{
    for (auto tx : txs) {
        delete tx;
    }
    txs.resize(0);
    txs.shrink_to_fit();
}

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

Block* RejectedTXBlock::parse(std::string_view block_sw)
{
    if (block_sw.size() < 80) {
        DEBUG_COUT("if (size < 80)");
        delete this;
        return nullptr;
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
            delete this;
            return nullptr;
        }
        cur_pos += varint_size;
        sign_start = cur_pos;

        if (cur_pos + sign_size >= block_sw.size()) {
            DEBUG_COUT("corrupt sign size");
            delete this;
            return nullptr;
        }
        tmp_sign = std::string_view(&block_sw[cur_pos], sign_size);
        cur_pos += sign_size;
    }

    {
        std::string_view varint_arr(&block_sw[cur_pos], block_sw.size() - cur_pos);
        uint64_t varint_size = read_varint(pubk_size, varint_arr);
        if (varint_size < 1) {
            DEBUG_COUT("corrupt varint size");
            delete this;
            return nullptr;
        }
        cur_pos += varint_size;
        pubk_start = cur_pos;

        if (cur_pos + pubk_size >= block_sw.size()) {
            DEBUG_COUT("corrupt pubk size");
            delete this;
            return nullptr;
        }
        tmp_pubk = std::string_view(&block_sw[cur_pos], pubk_size);
        cur_pos += pubk_size;
    }

    data_start = cur_pos;
    data_size = block_sw.size() - cur_pos;
    tmp_data_for_sign = std::string_view(&block_sw[data_start], data_size);

    if (!check_sign(tmp_data_for_sign, tmp_sign, tmp_pubk)) {
        DEBUG_COUT("Invalid Block Sign");
        delete this;
        return nullptr;
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
            delete this;
            return nullptr;
        }
        cur_pos += varint_size;
    }

    while (tx_size > 0) {
        if (cur_pos + tx_size >= block_sw.size()) {
            DEBUG_COUT("TX BUFF ERROR");
            delete this;
            return nullptr;
        }

        std::string_view tx_as_sw(&block_sw[cur_pos], tx_size);
        auto* tx = new RejectedTXInfo;
        if (tx->parse(tx_as_sw)) {
            txs.push_back(tx);
        } else {
            DEBUG_COUT("TX PARSE ERROR");
            delete tx;
            delete this;
            return nullptr;
        }
        cur_pos += tx_size;

        {
            std::string_view tx_size_arr = std::string_view(
                block_sw.begin() + cur_pos,
                block_sw.size() - cur_pos);
            uint64_t varint_size = read_varint(tx_size, tx_size_arr);
            if (varint_size < 1) {
                DEBUG_COUT("VARINT READ ERROR");
                delete this;
                return nullptr;
            }
            cur_pos += varint_size;
        }
    }

    data.insert(data.end(), block_sw.begin(), block_sw.begin() + cur_pos);

    sign = std::string_view(&data[sign_start], sign_size);
    pub_key = std::string_view(&data[pubk_start], pubk_size);
    data_for_sign = std::string_view(&data[data_start], data_size);

    block_hash = get_sha256(data);

    return this;
}

void RejectedTXBlock::clean()
{
    for (auto tx : txs) {
        delete tx;
    }
    txs.resize(0);
    txs.shrink_to_fit();
}
