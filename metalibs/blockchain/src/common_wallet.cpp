#include <meta_log.hpp>
#include <open_ssl_decor.h>
#include <statics.hpp>
#include <wallet.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

CommonWallet::~CommonWallet()
{
    delete real_addition;
    delete addition;
}

uint64_t CommonWallet::get_balance()
{
    if (addition) {
        return (balance - addition->delegated_to_sum);
    }
    return balance;
}

CommonWallet::CommonWallet(std::unordered_set<Wallet*>& _changed_wallets)
    : Wallet(_changed_wallets)
{
}

uint64_t CommonWallet::get_delegated_from_sum()
{
    if (addition) {
        return addition->delegated_from_sum;
    }
    return 0;
}

uint64_t CommonWallet::get_state()
{
    if (addition) {
        return addition->state;
    }
    return 0;
}

void CommonWallet::set_state(uint64_t new_state)
{
    if (!addition) {
        addition = new WalletAdditions();
    }
    addition->state = new_state;
    changed_wallets.insert(this);
}

uint64_t CommonWallet::get_trust()
{
    if (addition) {
        return addition->trust;
    }

    return 2;
}

void CommonWallet::add_trust()
{
    if (!addition) {
        addition = new WalletAdditions();
    }
    if (addition->trust < 200) {
        addition->trust++;
    } else {
        addition->trust = 200;
    }
    changed_wallets.insert(this);
}

void CommonWallet::sub_trust()
{
    if (!addition) {
        addition = new WalletAdditions();
    }
    if (addition->trust >= 12) {
        addition->trust -= 10;
    } else {
        addition->trust = 2;
    }
    changed_wallets.insert(this);
}

std::deque<std::pair<std::string, uint64_t>> CommonWallet::get_delegate_to_list()
{
    std::deque<std::pair<std::string, uint64_t>> return_list;
    if (addition) {
        for (const auto& delegate_to_pair : addition->delegate_to_daly_snapshot) {
            return_list.emplace_back(delegate_to_pair);
        }
    }

    return return_list;
}

std::deque<std::pair<std::string, uint64_t>> CommonWallet::get_delegated_from_list()
{
    std::deque<std::pair<std::string, uint64_t>> return_list;
    if (addition) {
        for (const auto& delegated_from_pair : addition->delegated_from_daly_snapshot) {
            return_list.emplace_back(delegated_from_pair);
        }
    }

    return return_list;
}

bool CommonWallet::try_delegate(Wallet* other, TX const* tx)
{
    const auto& addr_to = tx->addr_to;
    const auto& addr_from = tx->addr_from;
    const auto& parameters = tx->json_rpc->parameters;

    auto* wallet_to = dynamic_cast<CommonWallet*>(other);
    if (!wallet_to) {
        DEBUG_COUT("invalid wallet type");
        return false;
    }

    if (parameters.find("value") == parameters.end()) {
        DEBUG_COUT("#json error: no value in method parameters");
        return false;
    }

    uint64_t value = 0;
    try {
        value = std::stoul(parameters.at("value"));
    } catch (...) {
        DEBUG_COUT("#json data error: invalid value");
        return false;
    }

    if (get_balance() < value) {
        DEBUG_COUT("delegate failed");
        DEBUG_COUT("insufficent money");
        DEBUG_COUT(std::to_string(value));
        return false;
    }

    uint64_t MINIMUM_COIN_FORGING = 0;
    if (tx->addr_to == MASTER_WALLET_COIN_FORGING) {
        MINIMUM_COIN_FORGING = MINIMUM_COIN_FORGING_W;
    } else if (tx->addr_to == MASTER_WALLET_NODE_FORGING) {
        MINIMUM_COIN_FORGING = MINIMUM_COIN_FORGING_N;
    } else {
        MINIMUM_COIN_FORGING = MINIMUM_COIN_FORGING_C;
    }

    if (value < MINIMUM_COIN_FORGING) {
        DEBUG_COUT("delegate failed");
        DEBUG_COUT("too small value");
        DEBUG_COUT(std::to_string(value));
        return false;
    }

    if (get_balance() < value) {
        DEBUG_COUT("get_balance() < value");
        DEBUG_COUT(std::to_string(get_balance()) + "\t<\t" + std::to_string(value));
        return false;
    }

    if (addition && addition->delegate_to.size() >= LIMIT_DELEGATE_TO) {
        DEBUG_COUT("> LIMIT_DELEGATE_TO");
        return false;
    }

    if (wallet_to->addition && wallet_to->addition->delegated_from.size() >= LIMIT_DELEGATE_FROM) {
        if (addr_to != MASTER_WALLET_COIN_FORGING
            && addr_to != MASTER_WALLET_NODE_FORGING) {

            DEBUG_COUT("> LIMIT_DELEGATE_FROM");
            return false;
        }
    }

    if (!addition) {
        addition = new WalletAdditions();
    }
    if (!wallet_to->addition) {
        wallet_to->addition = new WalletAdditions();
    }

    addition->delegate_to.emplace_back(addr_to, value);
    addition->delegated_to_sum += value;

    wallet_to->addition->delegated_from.emplace_back(addr_from, value);
    wallet_to->addition->delegated_from_sum += value;

    changed_wallets.insert(this);
    changed_wallets.insert(wallet_to);
    return true;
}

bool CommonWallet::try_undelegate(Wallet* other, TX const* tx)
{
    const auto& addr_to = tx->addr_to;
    const auto& addr_from = tx->addr_from;

    auto* wallet_to = dynamic_cast<CommonWallet*>(other);
    if (!wallet_to) {
        DEBUG_COUT("invalid wallet type");
        return false;
    }

    if (!addition) {
        DEBUG_COUT("!addition");
        return false;
    }

    if (!wallet_to->addition) {
        DEBUG_COUT("!other.addition");
        return false;
    }

    int64_t i_to = addition->delegate_to.size() - 1;

    for (; i_to >= 0; i_to--) {
        std::string f_addr;
        uint64_t f_value;
        std::tie(f_addr, f_value) = addition->delegate_to[i_to];

        if (f_addr == addr_to) {
            break;
        }
    }
    if (i_to < 0) {
        DEBUG_COUT("no addr_to");
        return false;
    }

    int64_t i_from = wallet_to->addition->delegated_from.size() - 1;
    for (; i_from >= 0; i_from--) {
        std::string f_addr;
        uint64_t f_value;
        std::tie(f_addr, f_value) = wallet_to->addition->delegated_from[i_from];

        if (f_addr == addr_from) {
            break;
        }
    }
    if (i_from < 0) {
        DEBUG_COUT("no addr_from");
        return false;
    }

    addition->delegated_to_sum -= addition->delegate_to[i_to].second;
    addition->delegate_to.erase(addition->delegate_to.begin() + i_to);

    wallet_to->addition->delegated_from_sum -= wallet_to->addition->delegated_from[i_from].second;
    wallet_to->addition->delegated_from.erase(wallet_to->addition->delegated_from.begin() + i_from);

    changed_wallets.insert(this);
    changed_wallets.insert(wallet_to);
    return true;
}

bool CommonWallet::register_node(Wallet*, const TX* tx)
{
    const auto& method = tx->json_rpc->method;
    uint64_t w_state = get_state();

    auto&& set_state_by_type = [&w_state](const std::string& type) {
        if (type == "Proxy") {
            w_state |= NODE_STATE_FLAG_PROXY_PRETEND;
        } else if (type == "InfrastructureTorrent") {
            w_state |= NODE_STATE_FLAG_INFRASTRUCTURETORRENT_PRETEND;
        } else if (type == "Torrent") {
            w_state |= NODE_STATE_FLAG_TORRENT_PRETEND;
        } else if (type == "Verifier") {
            w_state |= NODE_STATE_FLAG_VERIFIER_PRETEND;
        } else if (type == "Core") {
            w_state |= NODE_STATE_FLAG_CORE_PRETEND;
        } else if (type == "Auto") {
            w_state |= NODE_STATE_FLAG_PROXY_PRETEND;
            w_state |= NODE_STATE_FLAG_INFRASTRUCTURETORRENT_PRETEND;
            w_state |= NODE_STATE_FLAG_TORRENT_PRETEND;
            w_state |= NODE_STATE_FLAG_VERIFIER_PRETEND;
            w_state |= NODE_STATE_FLAG_CORE_PRETEND;
        }
    };

    if (method == "mh-noderegistration") {
        w_state |= NODE_STATE_FLAG_PROXY_PRETEND;
    } else if (tx->json_rpc->parameters.find("type") != tx->json_rpc->parameters.end()) {
        const auto& type = tx->json_rpc->parameters["type"];
        auto start = 0U;
        auto&& end = type.find('|');
        while (end != std::string::npos) {
            set_state_by_type(type.substr(start, end - start));
            start = end + 1;
            end = type.find('|', start);
        }

        set_state_by_type(type.substr(start, end));
    } else {
        w_state |= NODE_STATE_FLAG_PROXY_PRETEND;
    }

    set_state(w_state);

    return true;
}

void CommonWallet::apply_delegates()
{
    if (addition) {
        changed_wallets.insert(this);

        addition->delegated_from_daly_snapshot = addition->delegated_from;
        addition->delegate_to_daly_snapshot = addition->delegate_to;

        if (addition->founder) {
            addition->limit += FORGING_DAILY_LIMIT_UP;
            return;
        }
        if (addition->state) {
            return;
        }
        if (addition->trust != 2) {
            return;
        }

        if (addition->delegated_from_sum) {
            return;
        }
        if (addition->delegated_to_sum) {
            return;
        }

        if (!addition->delegated_from.empty()) {
            return;
        }
        if (!addition->delegate_to.empty()) {
            return;
        }

        if (!addition->delegated_from_daly_snapshot.empty()) {
            return;
        }
        if (!addition->delegate_to_daly_snapshot.empty()) {
            return;
        }

        delete addition;
        addition = nullptr;
    }
}

std::tuple<uint64_t, uint64_t, std::string> CommonWallet::serialize()
{
    std::string data;

    if (addition) {
        bool has_json = false;

        rapidjson::StringBuffer s;
        rapidjson::Writer<rapidjson::StringBuffer> writer(s);

        writer.StartObject();

        if (addition->founder) {
            has_json = true;
            writer.String("used_limit");
            writer.Uint64(addition->used_limit);

            writer.String("limit");
            writer.Uint64(addition->limit);
        }

        if (addition->state || addition->trust != 2) {
            has_json = true;
            writer.String("state");
            writer.Uint64(get_state());

            writer.String("trust");
            writer.Uint64(get_trust() * 5);
        }

        if (addition->delegate_to_daly_snapshot.size()) {
            has_json = true;
            writer.String("delegate_to");
            writer.StartArray();
            for (auto&& [a, v] : addition->delegate_to_daly_snapshot) {
                writer.StartObject();
                writer.String("a");
                writer.String(a.c_str());
                writer.String("v");
                writer.Uint64(v);
                writer.EndObject();
            }
            writer.EndArray();
        }

        if (addition->delegated_from_daly_snapshot.size()) {
            has_json = true;
            writer.String("delegated_from");
            writer.StartArray();
            for (auto&& [a, v] : addition->delegated_from_daly_snapshot) {
                writer.StartObject();
                writer.String("a");
                writer.String(a.c_str());
                writer.String("v");
                writer.Uint64(v);
                writer.EndObject();
            }
            writer.EndArray();
        }

        writer.EndObject();

        if (has_json) {
            data = std::string(s.GetString());
        }
    }

    return { balance, transaction_id, data };
}

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

    changed_wallets.insert(this);
    return true;
}

uint64_t CommonWallet::sub(Wallet* other, TX const* tx, uint64_t real_fee)
{
    uint64_t total_sub = tx->value + real_fee;

    if (get_balance() < total_sub) {
        DEBUG_COUT("insuficent funds");
        return TX_REJECT_INSUFFICIENT_FUNDS_EXT;
    }

    if (addition && addition->founder && addition->used_limit + total_sub > addition->limit) {
        DEBUG_COUT("founder limits");
        return TX_REJECT_FOUNDER_LIMIT;
    }

    uint64_t sub_result = Wallet::sub(other, tx, real_fee);

    if (addition && addition->founder && sub_result == 0) {
        addition->used_limit += total_sub;
    }

    return sub_result;
}

bool CommonWallet::try_apply_method(Wallet* other, TX const* tx)
{
    if (!tx->json_rpc) {
        //        DEBUG_COUT("no json present");
        return true;
    }

    const auto& method = tx->json_rpc->method;

    if (method == "delegate") {
        if (!try_delegate(other, tx)) {
            DEBUG_COUT("delegate failed");
            return false;
        }
    } else if (method == "undelegate") {
        if (!try_undelegate(other, tx)) {
            DEBUG_COUT("#undelegate failed");
            return false;
        }
    } else if (method == "mhRegisterNode") {
        if (!register_node(other, tx)) {
            DEBUG_COUT("#mhRegisterNode failed");
            return false;
        }
    } else if (method == "mh-noderegistration") {
        if (!register_node(other, tx)) {
            DEBUG_COUT("#mh-noderegistration failed");
            return false;
        }
    } else if (method == "dapp_create") {
        auto* wallet_to = dynamic_cast<DecentralizedApplication*>(other);
        if (!wallet_to) {
            DEBUG_COUT("invalid wallet type");
            return false;
        }

        return wallet_to->try_dapp_create(tx);
    } else if (method == "dapp_modify") {
        auto* wallet_to = dynamic_cast<DecentralizedApplication*>(other);
        if (!wallet_to) {
            DEBUG_COUT("invalid wallet type");
            return false;
        }

        return wallet_to->try_dapp_modify(tx);
    } else if (method == "dapp_add_host") {
        auto* wallet_to = dynamic_cast<DecentralizedApplication*>(other);
        if (!wallet_to) {
            DEBUG_COUT("invalid wallet type");
            return false;
        }

        return wallet_to->try_dapp_add_host(tx);
    } else if (method == "dapp_del_host") {
        auto* wallet_to = dynamic_cast<DecentralizedApplication*>(other);
        if (!wallet_to) {
            DEBUG_COUT("invalid wallet type");
            return false;
        }
        return wallet_to->try_dapp_del_host(tx);
    }

    return true;
}

void CommonWallet::apply()
{
    delete real_addition;

    if (addition) {
        real_addition = new WalletAdditions(*addition);
    } else {
        real_addition = nullptr;
    }

    Wallet::apply();
}

void CommonWallet::clear()
{
    delete addition;

    if (real_addition) {
        addition = new WalletAdditions(*real_addition);
    } else {
        addition = nullptr;
    }

    Wallet::clear();
}
void CommonWallet::set_trust(uint64_t new_trust)
{
    if (!addition) {
        addition = new WalletAdditions();
    }
    addition->trust = new_trust;

    changed_wallets.insert(this);
}