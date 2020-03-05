#include <meta_log.hpp>
#include <open_ssl_decor.h>
#include <statics.hpp>
#include <wallet.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

DecentralizedApplication::DecentralizedApplication(std::unordered_set<Wallet*>& _changed_wallets)
    : Wallet(_changed_wallets)
{
}

bool DecentralizedApplication::initialize(uint64_t nonce)
{
    if (init_nonce) {
        return false;
    }

    init_nonce = nonce;

    changed_wallets.insert(this);
    return true;
}

std::tuple<uint64_t, uint64_t, std::string> DecentralizedApplication::serialize()
{
    std::string data;

    if (init_nonce) {
        rapidjson::StringBuffer s;
        rapidjson::Writer<rapidjson::StringBuffer> writer(s);

        writer.StartObject();

        writer.String("init");
        writer.Uint64(init_nonce);

        writer.String("mhc_per_day");
        writer.Uint64(mhc_per_day);

        writer.String("hosts");
        writer.StartArray();
        for (auto& host_pair : host) {
            writer.StartObject();
            writer.String("a");
            writer.String(host_pair.first.c_str());
            writer.String("t");
            writer.Uint64(host_pair.second);
            writer.EndObject();
        }
        writer.EndArray();

        writer.EndObject();

        data = std::string(s.GetString());
    }

    return { balance, transaction_id, data };
}

bool DecentralizedApplication::initialize(uint64_t value, uint64_t nonce, const std::string& json)
{
    Wallet::initialize(value, nonce, "");

    if (!json.empty() && json.front() == '{' && json.back() == '}') {
        rapidjson::Document rpc_json;
        if (!rpc_json.Parse(json.c_str()).HasParseError()) {
            if (rpc_json.HasMember("init") && rpc_json["init"].GetUint64()) {
                init_nonce = rpc_json["init"].GetUint64();
            }

            if (rpc_json.HasMember("mhc_per_day") && rpc_json["mhc_per_day"].GetUint64()) {
                mhc_per_day = rpc_json["mhc_per_day"].GetUint64();
            }

            if (rpc_json.HasMember("hosts") && rpc_json["hosts"].IsArray()) {
                auto& d_list = rpc_json["hosts"];

                for (uint i = 0; i < d_list.Size(); i++) {
                    auto& record = d_list[i];

                    if (record.HasMember("a") && record["a"].IsString() && record.HasMember("t") && record["t"].IsUint64()) {
                        host.insert({ std::string(record["a"].GetString(), record["a"].GetStringLength()), record["t"].GetUint64() });
                    } else {
                        DEBUG_COUT("invalid pair");
                        DEBUG_COUT(json);
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

    apply();

    return true;
}

bool DecentralizedApplication::try_apply_method(Wallet* , TX const* )
{
    return false;
}

bool DecentralizedApplication::try_dapp_create(const TX* tx)
{
    const auto& addr_to = tx->addr_to;
    const auto& addr_from = tx->addr_from;
    const auto& parameters = tx->json_rpc->parameters;

    if (parameters.find("mhc_per_day") == parameters.end()) {
        DEBUG_COUT("#json error: no mhc_per_day in method parameters");
        return false;
    }

    uint64_t value = 0;
    try {
        value = std::stoul(parameters.at("mhc_per_day"));
    } catch (...) {
        DEBUG_COUT("#json data error: invalid mhc_per_day");
        return false;
    }

    std::string dapp_addr = get_contract_address(addr_from, tx->nonce);
    if (addr_to != dapp_addr) {
        DEBUG_COUT("not an owner");
        return false;
    }

    if (!initialize(tx->nonce)) {
        DEBUG_COUT("already initialized");
        return false;
    }

    mhc_per_day = value;

    changed_wallets.insert(this);
    return true;
}

bool DecentralizedApplication::try_dapp_modify(const TX* tx)
{
    const auto& addr_to = tx->addr_to;
    const auto& addr_from = tx->addr_from;
    const auto& parameters = tx->json_rpc->parameters;

    if (parameters.find("mhc_per_day") == parameters.end()) {
        DEBUG_COUT("#json error: no mhc_per_day in method parameters");
        return false;
    }

    uint64_t value = 0;
    try {
        value = std::stoul(parameters.at("mhc_per_day"));
    } catch (...) {
        DEBUG_COUT("#json data error: invalid mhc_per_day");
        return false;
    }

    std::string dapp_addr = get_contract_address(addr_from, init_nonce);
    if (addr_to != dapp_addr) {
        DEBUG_COUT("not an owner");
        return false;
    }

    mhc_per_day = value;

    changed_wallets.insert(this);
    return true;
}

bool DecentralizedApplication::try_dapp_add_host(const TX* tx)
{
    uint64_t timestamp = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count());

    changed_wallets.insert(this);
    return host.insert({ tx->addr_from, timestamp }).second;
}

bool DecentralizedApplication::try_dapp_del_host(const TX* tx)
{
    changed_wallets.insert(this);
    return host.erase(tx->addr_from);
}

std::map<std::string, uint64_t> DecentralizedApplication::make_dapps_host_rewards_list()
{
    static const uint64_t min_timestamp_diff = 60 * 60 * 24;
    std::map<std::string, uint64_t> return_map;

    if (balance < mhc_per_day) {
        return return_map;
    }

    uint64_t timestamp = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count());
    std::set<std::string> host_for_reward;
    for (const auto& host_pair : host) {
        if (host_pair.second - timestamp >= min_timestamp_diff) {
            return_map.insert({ host_pair.first, 0 });
        }
    }

    if (return_map.empty()) {
        return return_map;
    }

    uint64_t reward_per_one = mhc_per_day / return_map.size();
    for (auto& host_pair : return_map) {
        host_pair.second = reward_per_one;
    }

    return return_map;
}

void DecentralizedApplication::apply()
{
    real_mhc_per_day = mhc_per_day;
    real_host = host;

    Wallet::apply();
}

void DecentralizedApplication::clear()
{
    mhc_per_day = real_mhc_per_day;
    host = real_host;

    Wallet::clear();
}