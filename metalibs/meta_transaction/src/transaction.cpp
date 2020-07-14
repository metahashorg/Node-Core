#include <meta_log.hpp>
#include <meta_transaction.h>

namespace metahash::transaction {

bool read_varint(std::vector<char>& data, uint64_t& index, uint64_t& varint)
{
    uint8_t varint_size;
    std::string_view varint_arr(&data[index], data.size() - index);
    varint_size = crypto::read_varint(varint, varint_arr);
    if (varint_size < 1) {
        DEBUG_COUT("corrupt varint size");
        return false;
    }
    index += varint_size;
    return true;
}

bool TX::fill_from_strings(
    std::string& param_to,
    std::string param_value,
    std::string param_fee,
    std::string param_nonce,
    std::string& param_data,
    std::string& param_sign,
    std::string& param_pub_key)
{
    unsigned long transaction_value = 0;
    unsigned long transaction_id = 0;
    unsigned long transaction_fee = 0;

    if (param_value.empty()) {
        param_value = "0";
    }
    try {
        transaction_value = std::stoul(param_value);
    } catch (...) {
        DEBUG_COUT("invalid transaction_value");
        return false;
    }

    if (param_nonce.empty()) {
        param_nonce = "0";
    }
    try {
        transaction_id = std::stoul(param_nonce);
    } catch (...) {
        DEBUG_COUT("invalid transaction_id");
        return false;
    }

    if (param_fee.empty()) {
        param_fee = "0";
    }
    try {
        transaction_fee = std::stoul(param_fee);
    } catch (...) {
        DEBUG_COUT("invalid transaction_fee");
        return false;
    }

    std::vector<unsigned char> bin_to = crypto::hex2bin(param_to);
    std::vector<unsigned char> bin_data = crypto::hex2bin(param_data);
    std::vector<unsigned char> bin_sign = crypto::hex2bin(param_sign);
    std::vector<unsigned char> bin_pub_key = crypto::hex2bin(param_pub_key);

    return fill_sign_n_raw(bin_to, transaction_value, transaction_fee, transaction_id, bin_data, bin_sign, bin_pub_key);
}

void TX::clear()
{
    bin_to = std::string_view();
    value = 0;
    fee = 0;
    nonce = 0;
    data = std::string_view();
    sign = std::string_view();
    pub_key = std::string_view();

    data_for_sign = std::string_view();
    raw_tx.clear();
    hash = { 0 };

    addr_from.clear();
    addr_to.clear();
}

bool TX::check_tx()
{
    if (crypto::check_sign(data_for_sign, sign, pub_key)) {
        hash = crypto::get_sha256(raw_tx);
        addr_to = "0x" + crypto::bin2hex(bin_to);
        auto bin_from = crypto::get_address(pub_key);
        addr_from = "0x" + crypto::bin2hex(bin_from);
        return true;
    }

    DEBUG_COUT("check sign failed");
    return false;
}

}