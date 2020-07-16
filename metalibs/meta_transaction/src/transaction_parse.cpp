#include <rapidjson/document.h>

#include <meta_constants.hpp>
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

bool TX::parse(std::string_view raw_data, bool check_sign_flag)
{
    raw_tx.insert(raw_tx.end(), raw_data.begin(), raw_data.end());

    uint64_t index = 0;
    uint8_t varint_size;
    uint64_t sign_data_size = 0;

    {
        const uint8_t toadr_size = 25;
        if (index + toadr_size >= raw_tx.size()) {
            DEBUG_COUT("corrupt addres size");
            return false;
        }
        bin_to = std::string_view(&raw_tx[index], toadr_size);
        index += toadr_size;
    }

    if (!read_varint(raw_tx, index, value)) {
        DEBUG_COUT("corrupt value");
        return false;
    }

    if (!read_varint(raw_tx, index, fee)) {
        DEBUG_COUT("corrupt fee");
        return false;
    }

    if (!read_varint(raw_tx, index, nonce)) {
        DEBUG_COUT("corrupt nonce");
        return false;
    }

    {
        uint64_t data_size;
        if (!read_varint(raw_tx, index, data_size)) {
            DEBUG_COUT("corrupt data_size");
            return false;
        }

        if (index + data_size >= raw_tx.size()) {
            DEBUG_COUT("corrupt data size");
            return false;
        }
        data = std::string_view(&raw_tx[index], data_size);
        index += data_size;

        sign_data_size = index;
    }

    {
        uint64_t sign_size;
        if (!read_varint(raw_tx, index, sign_size)) {
            DEBUG_COUT("corrupt sign_size");
            return false;
        }

        if (index + sign_size >= raw_tx.size()) {
            DEBUG_COUT("corrupt sign size");
            return false;
        }
        sign = std::string_view(&raw_tx[index], sign_size);
        index += sign_size;
    }

    {
        uint64_t pubk_size;
        if (!read_varint(raw_tx, index, pubk_size)) {
            DEBUG_COUT("corrupt pubk_size");
            return false;
        }

        if (pubk_size && (index + pubk_size > raw_tx.size())) {
            DEBUG_COUT("corrupt pub_key size");
            return false;
        }
        pub_key = std::string_view(&raw_tx[index], pubk_size);
        index += pubk_size;
    }

    tx_size = index;

    if (index < raw_tx.size()) {
        std::string_view varint_arr(&raw_tx[index], raw_tx.size() - index);
        varint_size = crypto::read_varint(state, varint_arr);
        if (varint_size < 1) {
            DEBUG_COUT("corrupt varint size - could not read tx state");
        }
    }

    {
        data_for_sign = std::string_view(&raw_tx[0], sign_data_size);
        hash = crypto::get_sha256(raw_tx);
    }

    check_sign_flag = state != TX_STATE_FEE && check_sign_flag;
    if (check_sign_flag && !crypto::check_sign(data_for_sign, sign, pub_key)) {
        DEBUG_COUT("invalid sign");
        return false;
    }

    addr_to = "0x" + crypto::bin2hex(bin_to);
    if (check_sign_flag) {
        auto bin_from = crypto::get_address(pub_key);
        addr_from = "0x" + crypto::bin2hex(bin_from);
    }

    if (state != TX_STATE_APPROVE) {
        if (!data.empty() && data.front() == '{' && data.back() == '}') {
            std::string json_probably(data);
            rapidjson::Document rpc_json;
            if (!rpc_json.Parse(json_probably.c_str()).HasParseError()) {
                if (rpc_json.HasMember("method") && rpc_json["method"].IsString()) {
                    json_rpc = new JSON_RPC();
                    json_rpc->method = std::string(rpc_json["method"].GetString(), rpc_json["method"].GetStringLength());
                    if (rpc_json.HasMember("params") && rpc_json["params"].IsObject()) {
                        const rapidjson::Value& params = rpc_json["params"];
                        for (rapidjson::Value::ConstMemberIterator iter = params.MemberBegin(); iter != params.MemberEnd(); ++iter) {
                            if (iter->name.IsString() && iter->value.IsString()) {
                                json_rpc->parameters[std::string(iter->name.GetString(), iter->name.GetStringLength())] = std::string(iter->value.GetString(), iter->value.GetStringLength());
                            } else {
                                DEBUG_COUT("invalid params");
                                DEBUG_COUT(data);
                            }
                        }
                    }
                }
            }
        }
    }

    return true;
}

}