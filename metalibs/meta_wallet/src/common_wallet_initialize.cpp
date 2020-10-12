#include <meta_log.hpp>
#include <meta_wallet.h>

#include <rapidjson/document.h>

namespace metahash::meta_wallet {

bool CommonWallet::initialize(uint64_t value, uint64_t nonce, const std::string& json)
{
    //    DEBUG_COUT("#");
    Wallet::initialize(value, nonce, "");

    uint64_t state = 0;
    uint64_t trust = 2;

    bool founder = false;
    uint64_t used_limit = 0;
    uint64_t limit = 0;

    std::deque<std::pair<std::string, uint64_t>> delegated_from;
    std::deque<std::pair<std::string, uint64_t>> delegate_to;

    uint64_t delegated_from_sum = 0;
    uint64_t delegated_to_sum = 0;

    bool got_additions = false;
    if (!json.empty() && json.front() == '{' && json.back() == '}') {
        rapidjson::Document rpc_json;
        if (!rpc_json.Parse(json.c_str()).HasParseError()) {
            if (rpc_json.HasMember("state") && rpc_json["state"].IsUint64()) {
                got_additions = true;
                state = rpc_json["state"].GetUint64();
            }

            if (rpc_json.HasMember("trust") && rpc_json["trust"].IsUint64()) {
                got_additions = true;
                trust = rpc_json["trust"].GetUint64() / 5;
            }

            if (rpc_json.HasMember("used_limit") && rpc_json["used_limit"].IsUint64()) {
                got_additions = true;
                founder = true;
                used_limit = rpc_json["used_limit"].GetUint64();
            }

            if (rpc_json.HasMember("limit") && rpc_json["limit"].IsUint64()) {
                got_additions = true;
                founder = true;
                limit = rpc_json["limit"].GetUint64();
            }

            if (rpc_json.HasMember("delegate_list") && rpc_json["delegate_list"].IsArray()) {
                got_additions = true;
                auto& d_list = rpc_json["delegate_list"];

                for (uint i = 0; i < d_list.Size(); i++) {
                    auto& record = d_list[i];

                    if (record.HasMember("a") && record["a"].IsString() && record.HasMember("v") && record["v"].IsUint64()) {
                        delegate_to.emplace_back(std::string(record["a"].GetString(), record["a"].GetStringLength()), record["v"].GetUint64());
                        delegated_to_sum += record["v"].GetUint64();
                    } else {
                        DEBUG_COUT("invalid pair");
                        DEBUG_COUT(json);
                    }
                }
            } else {
                if (rpc_json.HasMember("delegate_to") && rpc_json["delegate_to"].IsArray()) {
                    got_additions = true;
                    auto& d_list = rpc_json["delegate_to"];

                    for (uint i = 0; i < d_list.Size(); i++) {
                        auto& record = d_list[i];

                        if (record.HasMember("a") && record["a"].IsString() && record.HasMember("v") && record["v"].IsUint64()) {
                            delegate_to.emplace_back(std::string(record["a"].GetString(), record["a"].GetStringLength()), record["v"].GetUint64());
                            delegated_to_sum += record["v"].GetUint64();
                        } else {
                            DEBUG_COUT("invalid pair");
                            DEBUG_COUT(json);
                        }
                    }
                }
                if (rpc_json.HasMember("delegated_from") && rpc_json["delegated_from"].IsArray()) {
                    got_additions = true;
                    auto& d_list = rpc_json["delegated_from"];

                    for (uint i = 0; i < d_list.Size(); i++) {
                        auto& record = d_list[i];

                        if (record.HasMember("a") && record["a"].IsString() && record.HasMember("v") && record["v"].IsUint64()) {
                            delegated_from.emplace_back(std::string(record["a"].GetString(), record["a"].GetStringLength()), record["v"].GetUint64());
                            delegated_from_sum += record["v"].GetUint64();
                        } else {
                            DEBUG_COUT("invalid pair");
                            DEBUG_COUT(json);
                        }
                    }
                }
            }
        } else {
            DEBUG_COUT("json parse error");
            DEBUG_COUT("Error No");
            DEBUG_COUT(rpc_json.GetErrorOffset());
            DEBUG_COUT("message wth error");
            DEBUG_COUT(json);
        }
    }

    delete addition;
    addition = nullptr;

    if (got_additions) {
        delete addition;
        addition = new WalletAdditions;

        addition->founder = founder;
        addition->used_limit = used_limit;
        addition->limit = limit;

        addition->state = state;
        addition->trust = trust;

        addition->delegated_from = delegated_from;
        addition->delegated_from_daly_snapshot = delegated_from;

        addition->delegate_to = delegate_to;
        addition->delegate_to_daly_snapshot = delegate_to;

        addition->delegated_to_sum = delegated_to_sum;
        addition->delegated_from_sum = delegated_from_sum;
    }

    changed_wallets.push_back(this);
    return true;
}

}