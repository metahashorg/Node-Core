#include <meta_block.h>
#include <meta_constants.hpp>
#include <meta_crypto.h>
#include <meta_log.hpp>

namespace metahash::block {

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
        auto* tx = new transaction::RejectedTXInfo;
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

}