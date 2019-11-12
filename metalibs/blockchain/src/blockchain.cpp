#include <random>
#include <set>

#include <chain.h>
#include <meta_log.hpp>
#include <open_ssl_decor.h>
#include <statics.hpp>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

BlockChain::BlockChain() = default;

bool BlockChain::can_apply_block(Block* block)
{
    return try_apply_block(block, false);
}

bool BlockChain::apply_block(Block* block)
{
    if (try_apply_block(block, true)) {
        prev_hash = block->get_block_hash();
        if (block->get_block_type() == BLOCK_TYPE_STATE) {
            state_hash_xx64 = get_xxhash64(block->get_data());
        }
        return true;
    }
    DEBUG_COUT("block is corrupt");
    wallet_map.clear_changes();
    return false;
}

std::vector<char> make_forging_tx(const std::string& address, uint64_t reward, const std::vector<unsigned char>& data, uint64_t tx_type)
{
    std::vector<char> forging_tx;
    auto bin_addres = hex2bin(address);

    forging_tx.insert(forging_tx.end(), bin_addres.begin(), bin_addres.end());

    append_varint(forging_tx, reward);
    append_varint(forging_tx, 0);
    append_varint(forging_tx, 0);

    append_varint(forging_tx, data.size());
    forging_tx.insert(forging_tx.end(), data.begin(), data.end());

    append_varint(forging_tx, 0);
    append_varint(forging_tx, 0);

    append_varint(forging_tx, tx_type);

    return forging_tx;
}

Block* BlockChain::make_forging_block(uint64_t timestamp)
{
    uint64_t block_type = BLOCK_TYPE_FORGING;
    std::vector<char> txs_buff;

    Wallet* state_fee = wallet_map.get_wallet(STATE_FEE_WALLET);
    {
        static const uint64_t ROLE_UINT_PROXY = 1;
        static const uint64_t ROLE_UINT_TORRENT = 2;

        std::map<std::string, uint64_t> node_delegates;
        std::map<std::string, uint64_t> delegates;
        std::map<std::string, std::map<std::string, uint64_t>> geo_delegates = {
            { "us", std::map<std::string, uint64_t>() },
            { "eu", std::map<std::string, uint64_t>() },
            { "cn", std::map<std::string, uint64_t>() }
        };

        {
            DEBUG_COUT("proxy_statistics");
            std::string msg;
            for (auto& proxy_pair : proxy_statistics) {
                bool print = false;
                std::string addr_msg = "\"" + proxy_pair.first;
                for (const auto& geo_name : test_nodes) {
                    if (!proxy_pair.second.proxy_rps[geo_name.first].empty()) {
                        print = true;
                    }
                    addr_msg += ";" + geo_name.first;
                    addr_msg += ";" + std::to_string(proxy_pair.second.proxy_rps[geo_name.first].size());
                    {
                        auto& stat_v = proxy_pair.second.proxy_rps[geo_name.first];
                        auto geo_average = !stat_v.empty() ? std::accumulate(stat_v.begin(), stat_v.end(), 0) / stat_v.size() : 0;
                        addr_msg += ";" + std::to_string(geo_average);
                    }
                }
                if (print) {
                    msg += addr_msg;
                }
            }
            DEBUG_COUT(msg);
        }

        {
            DEBUG_COUT("torrent_statistics");
            std::string msg;
            for (auto& proxy_pair : proxy_statistics) {
                bool print = false;
                std::string addr_msg = "\"" + proxy_pair.first;
                for (const auto& geo_name : test_nodes) {
                    if (!proxy_pair.second.torrent_ping[geo_name.first].empty()) {
                        print = true;
                    }
                    addr_msg += ";" + geo_name.first;
                    addr_msg += ";" + std::to_string(proxy_pair.second.torrent_ping[geo_name.first].size());
                    {
                        auto& stat_v = proxy_pair.second.torrent_ping[geo_name.first];
                        auto geo_average = !stat_v.empty() ? std::accumulate(stat_v.begin(), stat_v.end(), 0) / stat_v.size() : UINT64_MAX;
                        addr_msg += ";" + std::to_string(geo_average);
                    }
                }
                if (print) {
                    msg += addr_msg;
                }
            }
            DEBUG_COUT(msg);
        }

        {

            for (auto& proxy_pair : proxy_statistics) {
                auto* wallet = dynamic_cast<CommonWallet*>(wallet_map.get_wallet(proxy_pair.first));
                if (!wallet) {
                    DEBUG_COUT("Invalid wallet");
                    continue;
                }

                uint64_t wallet_state(wallet->get_state());

                bool revard_set = false;
                std::string geo;
                uint64_t MAXIMUM_NODE_VALUE = 0;
                uint64_t role;

                DEBUG_COUT(proxy_pair.first + "\t" + int2bin(wallet_state));

                if (!revard_set && (wallet_state & NODE_STATE_FLAG_TORRENT_FORGING) == NODE_STATE_FLAG_TORRENT_FORGING) {
                    MAXIMUM_NODE_VALUE = NODE_TORRENT_HARD_CAP;
                    role = ROLE_UINT_TORRENT;
                    uint64_t average = UINT64_MAX;
                    uint64_t statistics_size = 0;

                    for (const auto& geo_name : test_nodes) {
                        auto& stat_v = proxy_pair.second.torrent_ping[geo_name.first];
                        auto geo_average = !stat_v.empty() ? std::accumulate(stat_v.begin(), stat_v.end(), 0) / stat_v.size() : UINT64_MAX;

                        statistics_size += stat_v.size();

                        if (geo_average < average) {
                            average = geo_average;
                            geo = geo_name.second;
                        }
                    }

                    uint64_t min_for_reward = proxy_pair.second.torrent_statistics_count * 95 / 100;
                    if (statistics_size < min_for_reward || average > 5000) {
                        DEBUG_COUT("not enough tests for torrent:\t" + proxy_pair.first + "\t" + std::to_string(min_for_reward) + "\t" + std::to_string(statistics_size));
                    } else {
                        revard_set = true;
                        DEBUG_COUT(proxy_pair.first + "\t" + std::to_string(average) + "\t" + geo + "\ttorrent");
                    }
                }
                if (!revard_set && (wallet_state & NODE_STATE_FLAG_PROXY_FORGING) == NODE_STATE_FLAG_PROXY_FORGING) {
                    MAXIMUM_NODE_VALUE = NODE_PROXY_HARD_CAP;
                    role = ROLE_UINT_PROXY;
                    uint64_t average = 0;
                    uint64_t statistics_size = 0;

                    for (const auto& geo_name : test_nodes) {
                        auto& stat_v = proxy_pair.second.proxy_rps[geo_name.first];
                        auto geo_average = !stat_v.empty() ? std::accumulate(stat_v.begin(), stat_v.end(), 0) / stat_v.size() : 0;

                        statistics_size += stat_v.size();

                        if (geo_average > average) {
                            average = geo_average;
                            geo = geo_name.second;
                        }
                    }

                    uint64_t min_for_reward = proxy_statistics[proxy_pair.first].proxy_statistics_count * 95 / 100;
                    if (statistics_size < min_for_reward || average < MINIMUM_AVERAGE_PROXY_RPS) {
                        DEBUG_COUT("not enough tests for proxy:\t" + proxy_pair.first + "\t" + std::to_string(min_for_reward) + "\t" + std::to_string(statistics_size));
                    } else {
                        revard_set = true;
                        DEBUG_COUT(proxy_pair.first + "\t" + std::to_string(average) + "\t" + geo + "\tproxy");
                    }
                }
                if (revard_set && node_delegates.find(proxy_pair.first) == node_delegates.end()) {
                    uint64_t node_total_delegate = 0;
                    for (auto& delegate_pair : wallet->get_delegated_from_list()) {
                        uint64_t node_wallet_delegate;

                        if (node_total_delegate + delegate_pair.second > MAXIMUM_NODE_VALUE) {
                            node_wallet_delegate = MAXIMUM_NODE_VALUE - node_total_delegate;
                        } else {
                            node_wallet_delegate = delegate_pair.second;
                        }

                        delegates[delegate_pair.first] += node_wallet_delegate;

                        node_total_delegate += node_wallet_delegate;
                        if (node_total_delegate >= MAXIMUM_NODE_VALUE) {
                            break;
                        }
                    }

                    geo_delegates[geo].insert({ proxy_pair.first, role });

                    if (node_total_delegate <= MAXIMUM_NODE_VALUE) {
                        node_delegates.insert({ proxy_pair.first, node_total_delegate });
                    } else {
                        node_delegates.insert({ proxy_pair.first, MAXIMUM_NODE_VALUE });
                    }
                }
            }
        }

        if (!node_delegates.empty()) {
            uint64_t forging_node_units = 0;
            std::map<uint64_t, std::map<std::string, uint64_t>> geo_total_units = {
                { ROLE_UINT_TORRENT,
                    { { "us", 0 },
                        { "eu", 0 },
                        { "cn", 0 } } },
                { ROLE_UINT_PROXY,
                    { { "us", 0 },
                        { "eu", 0 },
                        { "cn", 0 } } }
            };

            for (const auto& geo_pair : geo_delegates) {
                const auto& geo_name = geo_pair.first;
                for (const auto& node : geo_pair.second) {
                    const auto& node_name = node.first;
                    const auto& node_role = node.second;
                    uint64_t node_delegate = node_delegates[node_name];

                    geo_total_units[node_role][geo_name] += node_delegate;
                    forging_node_units += node_delegate;

                    DEBUG_COUT(node_name + "\t" + std::to_string(node_role) + "\t" + geo_name + "\t" + std::to_string(node_delegate));
                }
            }

            const uint64_t pool = (FORGING_POOL + state_fee->get_value());
            uint64_t forging_node_total = (pool * 28) / 100;

            std::map<uint64_t, std::map<std::string, uint64_t>> geo_total = {
                { ROLE_UINT_TORRENT,
                    { { "us", (pool * 2) / 100 },
                        { "eu", (pool * 2) / 100 },
                        { "cn", (pool * 2) / 100 } } },
                { ROLE_UINT_PROXY,
                    { { "us", (pool * 2) / 100 },
                        { "eu", (pool * 2) / 100 },
                        { "cn", (pool * 2) / 100 } } }
            };

            const double forging_node_per_unit = double(forging_node_total) / double(forging_node_units);
            const std::map<uint64_t, std::map<std::string, double>> geo_per_unit = {
                { ROLE_UINT_TORRENT,
                    { { "us", double(geo_total[ROLE_UINT_TORRENT]["us"]) / double(geo_total_units[ROLE_UINT_TORRENT]["us"]) },
                        { "eu", double(geo_total[ROLE_UINT_TORRENT]["eu"]) / double(geo_total_units[ROLE_UINT_TORRENT]["eu"]) },
                        { "cn", double(geo_total[ROLE_UINT_TORRENT]["cn"]) / double(geo_total_units[ROLE_UINT_TORRENT]["cn"]) } } },
                { ROLE_UINT_PROXY,
                    { { "us", double(geo_total[ROLE_UINT_PROXY]["us"]) / double(geo_total_units[ROLE_UINT_PROXY]["us"]) },
                        { "eu", double(geo_total[ROLE_UINT_PROXY]["eu"]) / double(geo_total_units[ROLE_UINT_PROXY]["eu"]) },
                        { "cn", double(geo_total[ROLE_UINT_PROXY]["cn"]) / double(geo_total_units[ROLE_UINT_PROXY]["cn"]) } } }
            };

            for (const auto& geo_pair : geo_delegates) {
                const auto& geo_name = geo_pair.first;
                DEBUG_COUT("GEO:\t" + geo_name);
                for (const auto& node : geo_pair.second) {
                    const auto& node_name = node.first;
                    const auto& node_role = node.second;
                    uint64_t node_delegate = node_delegates[node_name];

                    auto forging_node_reward = uint64_t(double(node_delegate) * forging_node_per_unit);
                    auto forging_node_reward_geo = uint64_t(double(node_delegate) * geo_per_unit.at(node_role).at(geo_name));

                    DEBUG_COUT("NODE_FORGING:\t" + node_name + "\t" + std::to_string(node_role) + "\t" + geo_name + "\t" + std::to_string(forging_node_reward) + "\t" + std::to_string(forging_node_reward_geo));

                    if (forging_node_total < forging_node_reward) {
                        DEBUG_COUT("Not enough money for reward in total");
                        DEBUG_COUT(std::to_string(forging_node_total));
                        DEBUG_COUT(std::to_string(forging_node_reward));
                        continue;
                    }
                    if (geo_total[node_role][geo_name] < forging_node_reward_geo) {
                        DEBUG_COUT("Not enough money for reward in geo");
                        DEBUG_COUT(std::to_string(geo_total[node_role][geo_name]));
                        DEBUG_COUT(std::to_string(forging_node_reward_geo));
                        continue;
                    }

                    geo_total[node_role][geo_name] -= forging_node_reward_geo;
                    forging_node_total -= forging_node_reward;

                    std::vector<char> state_tx = make_forging_tx(node_name, forging_node_reward + forging_node_reward_geo, {}, TX_STATE_FORGING_N);

                    if (!state_tx.empty()) {
                        append_varint(txs_buff, state_tx.size());
                        txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
                    }
                }
            }
        }

        uint64_t forging_coin_units = 0;
        for (auto& delegate_pair : delegates) {
            forging_coin_units += delegate_pair.second;
        }

        if (forging_coin_units) {
            uint64_t forging_coin_total = ((FORGING_POOL + state_fee->get_value()) * 5) / 10;
            double forging_coin_per_one = double(forging_coin_total) / double(forging_coin_units);

            for (auto& delegate_pair : delegates) {
                std::string coin_addres = delegate_pair.first;
                auto forging_coin = uint64_t(forging_coin_per_one * double(delegate_pair.second));

                if (forging_coin_total < forging_coin) {
                    continue;
                }

                forging_coin_total -= forging_coin;

                std::vector<char> state_tx = make_forging_tx(coin_addres, forging_coin, {}, TX_STATE_FORGING_C);

                if (!state_tx.empty()) {
                    append_varint(txs_buff, state_tx.size());
                    txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
                }
            }
        }
    }

    {
        auto* father_of_wallets = dynamic_cast<CommonWallet*>(wallet_map.get_wallet(MASTER_WALLET_COIN_FORGING));

        std::map<std::string, std::pair<int, int>>* address_statistics;
        while (true) {
            std::map<std::string, std::pair<int, int>>* null_stat = nullptr;
            address_statistics = wallet_statistics.load();
            if (wallet_statistics.compare_exchange_strong(address_statistics, null_stat)) {
                break;
            }
        }

        if (address_statistics) {
            DEBUG_COUT("online addr");
            std::string msg = "\t";
            for (const auto& add_pair : *address_statistics) {
                msg += add_pair.first + ":" + std::to_string(add_pair.second.first) + ":" + std::to_string(add_pair.second.second) + ";";
            }
            DEBUG_COUT(msg);
        }

        {
            DEBUG_COUT("delegate addr");
            std::string msg = "\t";
            for (const auto& add_pair : father_of_wallets->get_delegated_from_list()) {
                msg += add_pair.first + ":" + std::to_string(add_pair.second) + ";";
            }
            DEBUG_COUT(msg);
        }

        std::deque<std::string> active_forging;
        std::map<std::string, uint64_t> pasive_forging;

        if (address_statistics) {
            for (const auto& addr_stat : *address_statistics) {
                if (addr_stat.second.first) {
                    pasive_forging[addr_stat.first] = addr_stat.second.first;
                }
                if (addr_stat.second.second) {
                    pasive_forging[addr_stat.first] += 1;

                    for (uint i = 0; i < addr_stat.second.second; i++) {
                        active_forging.push_back(addr_stat.first);
                    }
                }
            }
        }

        {
            DEBUG_COUT("delegate list before sort");
            std::string msg = "\t";
            for (const auto& addr : active_forging) {
                msg += addr + ";";
            }
            DEBUG_COUT(msg);
        }

        if (!pasive_forging.empty()) {
            uint64_t forging_shares_total = 0;
            for (const auto& addr_pair : pasive_forging) {
                forging_shares_total += addr_pair.second;
            }

            const uint64_t forging_count_total = ((FORGING_POOL + state_fee->get_value()) * 1) / 10;

            const uint64_t FORGING_PASSIVE_REWARD = forging_count_total / 10;

            uint64_t reward_passive_per_share = FORGING_PASSIVE_REWARD / forging_shares_total;
            for (const auto& addr_pair : pasive_forging) {
                std::vector<char> state_tx = make_forging_tx(addr_pair.first, addr_pair.second * reward_passive_per_share, {}, TX_STATE_FORGING_W);
                append_varint(txs_buff, state_tx.size());
                txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
            }
        }

        if (!active_forging.empty()) {
            {
                DEBUG_COUT("state_hash_xx64\t" + std::to_string(state_hash_xx64));
                std::mt19937_64 r;
                r.seed(state_hash_xx64);
                std::shuffle(active_forging.begin(), active_forging.end(), r);
            }

            for (uint i = 0; i < 1000; i++) {
                if (active_forging.size() > i) {
                    auto addr = active_forging[i];
                    for (auto addr_it = active_forging.begin() + i + 1; addr_it != active_forging.end();) {
                        if (*addr_it == addr) {
                            addr_it = active_forging.erase(addr_it);
                        } else {
                            addr_it++;
                        }
                    }
                }
            }

            {
                DEBUG_COUT("delegate list after sort with uniques");
                std::string msg = "\t";
                for (const auto& addr : active_forging) {
                    msg += addr + ";";
                }
                DEBUG_COUT(msg);
            }

            const uint64_t forging_count_total = ((FORGING_POOL + state_fee->get_value()) * 1) / 10;

            const uint64_t FORGING_RANDOM_REWARD_1 = forging_count_total * 4 / 10;
            const uint64_t FORGING_RANDOM_REWARD_2 = forging_count_total * 1 / 10;
            const uint64_t FORGING_RANDOM_REWARD_3 = forging_count_total * 5 / 100;
            const uint64_t FORGING_RANDOM_REWARD_4 = forging_count_total * 415 / 10000;
            const uint64_t FORGING_RANDOM_REWARD_5 = forging_count_total * 335 / 10000;

            const uint64_t FORGING_RANDOM_REWARD_6_100 = forging_count_total * 1 / 1000;
            const uint64_t FORGING_RANDOM_REWARD_101_1000 = forging_count_total * 2 / 10000;

            uint64_t reward_bank = 0;
            if (active_forging.size() < 1000) {
                if (active_forging.size() >= 100) {
                    reward_bank += FORGING_RANDOM_REWARD_101_1000 * (1000 - active_forging.size());
                } else {
                    reward_bank += FORGING_RANDOM_REWARD_101_1000 * (1000 - 100);
                    if (active_forging.size() >= 6) {
                        reward_bank += FORGING_RANDOM_REWARD_6_100 * (100 - active_forging.size());
                    } else {
                        reward_bank += FORGING_RANDOM_REWARD_6_100 * (100 - 6);

                        if (active_forging.size() < 5) {
                            reward_bank += FORGING_RANDOM_REWARD_5;
                        }
                        if (active_forging.size() < 4) {
                            reward_bank += FORGING_RANDOM_REWARD_4;
                        }
                        if (active_forging.size() < 3) {
                            reward_bank += FORGING_RANDOM_REWARD_3;
                        }
                        if (active_forging.size() < 2) {
                            reward_bank += FORGING_RANDOM_REWARD_2;
                        }
                    }
                }
            }

            uint64_t reward_bank_per_one = reward_bank / active_forging.size();
            for (uint i = 0; i < 1 && i < active_forging.size(); i++) {
                std::vector<char> state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_1 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

                append_varint(txs_buff, state_tx.size());
                txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
            }
            for (uint i = 1; i < 2 && i < active_forging.size(); i++) {
                std::vector<char> state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_2 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

                append_varint(txs_buff, state_tx.size());
                txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
            }
            for (uint i = 2; i < 3 && i < active_forging.size(); i++) {
                std::vector<char> state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_3 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

                append_varint(txs_buff, state_tx.size());
                txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
            }
            for (uint i = 3; i < 4 && i < active_forging.size(); i++) {
                std::vector<char> state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_4 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

                append_varint(txs_buff, state_tx.size());
                txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
            }
            for (uint i = 4; i < 5 && i < active_forging.size(); i++) {
                std::vector<char> state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_5 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

                append_varint(txs_buff, state_tx.size());
                txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
            }
            for (uint i = 5; i < 100 && i < active_forging.size(); i++) {
                std::vector<char> state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_6_100 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

                append_varint(txs_buff, state_tx.size());
                txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
            }
            for (uint i = 100; i < 1000 && i < active_forging.size(); i++) {
                std::vector<char> state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_101_1000 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

                append_varint(txs_buff, state_tx.size());
                txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
            }
        }
    }

    {
        for (auto& wallet_pair : wallet_map) {
            auto* wallet = dynamic_cast<DecentralizedApplication*>(wallet_pair.second);
            if (!wallet) {
                continue;
            }

            std::map<std::string, uint64_t> reward_map = wallet->make_dapps_host_rewards_list();
            if (reward_map.empty()) {
                DEBUG_COUT("not a dapp or no money");
            }

            std::vector<unsigned char> bin_addr_from = hex2bin(wallet_pair.first);
            for (const auto& host_pair : reward_map) {
                std::vector<char> state_tx = make_forging_tx(host_pair.first, host_pair.second, bin_addr_from, TX_STATE_FORGING_DAPP);
            }
        }
    }

    txs_buff.push_back(0);

    return make_block(block_type, timestamp, prev_hash, txs_buff);
}

Block* BlockChain::make_state_block(uint64_t timestamp)
{
    const uint64_t block_type = BLOCK_TYPE_STATE;
    std::vector<char> txs_buff;
    static const std::vector<unsigned char> zero_bin_addres(25, 0x00);

    for (auto& wallet_pair : wallet_map) {

        if (!wallet_pair.second) {
            DEBUG_COUT("invalid wallet:\t" + wallet_pair.first);
            continue;
        }

        std::vector<char> state_tx;
        auto bin_addres = hex2bin(wallet_pair.first);

        if (bin_addres.size() != 25) {
            continue;
        }

        if (bin_addres == zero_bin_addres) {
            wallet_pair.second->initialize(0, 0, "");
        }

        uint64_t value = 0;
        uint64_t nonce = 0;
        std::string json;
        std::tie(value, nonce, json) = wallet_pair.second->serialize();

        state_tx.insert(state_tx.end(), bin_addres.begin(), bin_addres.end());

        append_varint(state_tx, value);
        append_varint(state_tx, 0);
        append_varint(state_tx, nonce);

        append_varint(state_tx, json.size());
        state_tx.insert(state_tx.end(), json.begin(), json.end());

        append_varint(state_tx, 0);
        append_varint(state_tx, 0);

        append_varint(state_tx, TX_STATE_STATE);

        append_varint(txs_buff, state_tx.size());
        txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
    }

    txs_buff.push_back(0);

    return make_block(block_type, timestamp, prev_hash, txs_buff);
}

Block* BlockChain::make_common_block(uint64_t timestamp, std::vector<TX*>& transactions)
{
    const uint64_t block_type = BLOCK_TYPE_COMMON;
    std::vector<char> txs_buff;
    Wallet* state_fee = wallet_map.get_wallet(STATE_FEE_WALLET);

    if (!transactions.empty()) {
        uint64_t fee = 0;
        uint64_t tx_aprove_count = 0;
        {
            fee = get_fee(transactions.size());

            std::vector<char> fee_tx;

            auto bin_addres = hex2bin(SPECIAL_WALLET_COMISSIONS);

            fee_tx.insert(fee_tx.end(), bin_addres.begin(), bin_addres.end());

            append_varint(fee_tx, fee);
            append_varint(fee_tx, 0);
            append_varint(fee_tx, 0);

            append_varint(fee_tx, 0);
            append_varint(fee_tx, 0);
            append_varint(fee_tx, 0);

            append_varint(fee_tx, TX_STATE_FEE);

            append_varint(txs_buff, fee_tx.size());
            txs_buff.insert(txs_buff.end(), fee_tx.begin(), fee_tx.end());
        }

        std::sort(transactions.begin(), transactions.end(), [](TX* lh, TX* rh) {
            return lh->nonce < rh->nonce;
        });

        //check temp balances
        for (auto* tx : transactions) {
            const std::string& addr_from = tx->addr_from;
            const std::string& addr_to = tx->addr_to;

            uint64_t state = 0;

            if (test_nodes.find(addr_from) != test_nodes.end() && tx->json_rpc) {
                statistics_tx_list.push_back(tx);
                continue;
            }
            if (addr_from == ZERO_WALLET) {
                //                DEBUG_COUT("invalid addr_from\t" + bin2hex(tx->hash));
                //                DEBUG_COUT(addr_from);
                //                DEBUG_COUT(addr_to);
                reject(tx, TX_REJECT_ZERO);
                delete tx;
                continue;
            }

            Wallet* wallet_to = wallet_map.get_wallet(addr_to);
            Wallet* wallet_from = wallet_map.get_wallet(addr_from);

            if (!wallet_to || !wallet_from) {
                //                DEBUG_COUT("invalid wallet\t" + bin2hex(tx->hash));
                //                DEBUG_COUT(addr_from);
                //                DEBUG_COUT(addr_to);
                reject(tx, TX_REJECT_INVALID_WALLET);
                delete tx;
                continue;
            }

            if (uint64_t status = wallet_from->sub(wallet_to, tx, fee + (tx->raw_tx.size() > 254 ? tx->raw_tx.size() - 254 : 0)) > 0) {
                //                DEBUG_COUT("tx hash:\t" + bin2hex(tx->hash));
                //                DEBUG_COUT("addr_from:\t" + addr_from);
                //                DEBUG_COUT("addr_to:\t" + addr_to);
                reject(tx, status);
                delete tx;
                continue;
            }

            state_fee->add(fee + (tx->raw_tx.size() > 254 ? tx->raw_tx.size() - 254 : 0));

            if (!wallet_from->try_apply_method(wallet_to, tx)) {
                state = TX_STATE_WRONG_DATA;
            }

            if (state == 0) {
                state = TX_STATE_ACCEPT;
            }

            {
                std::vector<unsigned char> state_as_varint_array = int_as_varint_array(state);
                append_varint(txs_buff, tx->raw_tx.size() + state_as_varint_array.size());
                txs_buff.insert(txs_buff.end(), tx->raw_tx.begin(), tx->raw_tx.end());
                txs_buff.insert(txs_buff.end(), state_as_varint_array.begin(), state_as_varint_array.end());
            }

            tx_aprove_count++;
            delete tx;
        }

        txs_buff.push_back(0);

        wallet_map.clear_changes();
        transactions.resize(0);

        if (tx_aprove_count == 0) {
            return nullptr;
        }

        return make_block(block_type, timestamp, prev_hash, txs_buff);
    }
    return nullptr;
}
void BlockChain::reject(const TX* tx, uint64_t reason)
{
    auto rejected_tx = new RejectedTXInfo();
    rejected_tx->make(tx->hash, reason);
    rejected_tx_list.push_back(rejected_tx);
}

Block* BlockChain::make_statistics_block(uint64_t timestamp)
{
    if (!statistics_tx_list.empty()) {
        uint64_t block_type = BLOCK_TYPE_COMMON;
        std::vector<char> txs_buff;

        std::sort(statistics_tx_list.begin(), statistics_tx_list.end(), [](TX* lh, TX* rh) {
            return lh->nonce < rh->nonce;
        });

        const uint64_t state = TX_STATE_TECH_NODE_STAT;
        const std::vector<unsigned char> state_as_varint_array = int_as_varint_array(state);
        for (TX* tx : statistics_tx_list) {
            append_varint(txs_buff, tx->raw_tx.size() + state_as_varint_array.size());
            txs_buff.insert(txs_buff.end(), tx->raw_tx.begin(), tx->raw_tx.end());
            txs_buff.insert(txs_buff.end(), state_as_varint_array.begin(), state_as_varint_array.end());

            delete tx;
        }
        statistics_tx_list.clear();
        txs_buff.push_back(0);

        return make_block(block_type, timestamp, prev_hash, txs_buff);
    }

    return nullptr;
}

std::atomic<std::map<std::string, std::pair<int, int>>*>& BlockChain::get_wallet_statistics()
{
    return wallet_statistics;
}

std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& BlockChain::get_wallet_request_addreses()
{
    return wallet_request_addreses;
}

Block* BlockChain::make_block(uint64_t b_type, uint64_t b_time, sha256_2 prev_b_hash, std::vector<char>& tx_buff)
{
    std::vector<char> block_buff;
    sha256_2 tx_hash = get_sha256(tx_buff);

    block_buff.insert(block_buff.end(), reinterpret_cast<char*>(&b_type), (reinterpret_cast<char*>(&b_type) + sizeof(uint64_t)));
    block_buff.insert(block_buff.end(), reinterpret_cast<char*>(&b_time), (reinterpret_cast<char*>(&b_time) + sizeof(uint64_t)));
    block_buff.insert(block_buff.end(), prev_b_hash.begin(), prev_b_hash.end());
    block_buff.insert(block_buff.end(), tx_hash.begin(), tx_hash.end());
    block_buff.insert(block_buff.end(), tx_buff.begin(), tx_buff.end());

    std::string_view block_as_sw(block_buff.data(), block_buff.size());
    Block* block = parse_block(block_as_sw);
    if (block) {
        DEBUG_COUT("BLOCK IS OK");
        return block;
    }
    return nullptr;
}

uint64_t BlockChain::get_fee(uint64_t cnt)
{
    static const uint64_t MAX_TRANSACTION_COUNT_20_of_100 = (MAX_TRANSACTION_COUNT * 20 / 100);
    if (cnt <= MAX_TRANSACTION_COUNT_20_of_100) {
        return COMISSION_COMMON_00_20;
    }
    if (cnt <= MAX_TRANSACTION_COUNT_20_of_100 * 2) {
        return COMISSION_COMMON_21_40;
    }
    if (cnt <= MAX_TRANSACTION_COUNT_20_of_100 * 3) {
        return COMISSION_COMMON_41_60;
    }
    if (cnt <= MAX_TRANSACTION_COUNT_20_of_100 * 4) {
        return COMISSION_COMMON_61_80;
    }
    return COMISSION_COMMON_81_99;
}

bool BlockChain::try_apply_block(Block* block, bool apply)
{
    static const sha256_2 zero_hash = { { 0 } };
    bool check_state = true;

    if (wallet_map.empty() && (block->get_block_type() == BLOCK_TYPE_STATE || block->get_prev_hash() == zero_hash)) {
        check_state = false;
        DEBUG_COUT("block is state or like\t" + bin2hex(block->get_block_hash()));
    } else if (block->get_prev_hash() != prev_hash) {
        DEBUG_COUT("prev hash not equal in block and database");
        return false;
    }

    bool status = false;
    switch (block->get_block_type()) {
    case BLOCK_TYPE_COMMON: {
        if (check_state) {
            status = can_apply_common_block(block);
        } else {
            status = can_apply_state_block(block, check_state);
        }
    } break;
    case BLOCK_TYPE_STATE: {
        status = can_apply_state_block(block, check_state);
    } break;
    case BLOCK_TYPE_FORGING: {
        status = can_apply_forging_block(block);
    } break;
    default:
        DEBUG_COUT("wrong block typo");
        return false;
    }

    if (status && apply) {
        wallet_map.apply_changes();
        return true;
    }

    wallet_map.clear_changes();
    return status;
}

bool BlockChain::can_apply_common_block(Block* block)
{

    auto common_block = dynamic_cast<CommonBlock*>(block);

    if (common_block) {
        uint64_t fee = 0;
        Wallet* state_fee = wallet_map.get_wallet(STATE_FEE_WALLET);

        for (const auto tx : common_block->get_txs()) {
            if (tx->state == TX_STATE_FEE) {
                fee = tx->value;
                continue;
            }

            const std::string& addr_from = tx->addr_from;
            const std::string& addr_to = tx->addr_to;

            Wallet* wallet_to = wallet_map.get_wallet(addr_to);
            Wallet* wallet_from = wallet_map.get_wallet(addr_from);

            if (!wallet_to || !wallet_from) {
                DEBUG_COUT("invalid wallet\t" + bin2hex(tx->hash));
                DEBUG_COUT(addr_from);
                DEBUG_COUT(addr_to);
                continue;
            }
            if (tx->state == TX_STATE_APPROVE) {
                wallet_from->sub(wallet_to, tx, 0);
                continue;
            }
            if (tx->state == TX_STATE_TECH_NODE_STAT && tx->json_rpc) {
                if (tx->json_rpc->parameters["type"] == "InfrastructureTorrent") {
                    auto& mhaddr = tx->json_rpc->parameters["address"];
                    proxy_statistics[mhaddr].torrent_statistics_count++;
                    uint64_t latency = 0;
                    try {
                        latency = std::stol(tx->json_rpc->parameters["latency"]);
                    } catch (...) {
                        latency = 0;
                    }

                    if (latency > 0 && latency < (1000l * 1000l)) {
                        proxy_statistics[mhaddr].torrent_ping[addr_from].push_back(latency);
                    }
                } else if (tx->json_rpc->parameters["type"] == "Proxy") {
                    auto& mhaddr = tx->json_rpc->parameters["address"];
                    proxy_statistics[mhaddr].proxy_statistics_count++;
                    uint64_t rps = 0;
                    try {
                        rps = std::stol(tx->json_rpc->parameters["rps"]);
                    } catch (...) {
                        rps = 0;
                    }

                    if (rps > MINIMUM_PROXY_RPS && rps < (1000l * 1000l)) {
                        proxy_statistics[mhaddr].proxy_rps[addr_from].push_back(rps);
                    }
                } else {
                    auto& mhaddr = tx->json_rpc->parameters["mhaddr"];
                    proxy_statistics[mhaddr].proxy_statistics_count++;

                    if (tx->json_rpc->parameters["success"] != "false") {

                        uint64_t rps = 0;
                        try {
                            rps = std::stol(tx->json_rpc->parameters["rps"]);
                        } catch (...) {
                            rps = 0;
                        }

                        if (rps > MINIMUM_PROXY_RPS && rps < (1000l * 1000l)) {
                            proxy_statistics[mhaddr].proxy_rps[addr_from].push_back(rps);
                        }
                    }
                }
                continue;
            }

            if (wallet_from->sub(wallet_to, tx, fee + (tx->raw_tx.size() > 255 ? tx->raw_tx.size() - 255 : 0)) > 0) {
                DEBUG_COUT("tx hash:\t" + bin2hex(tx->hash));
                DEBUG_COUT("addr_from:\t" + addr_from);
                DEBUG_COUT("addr_to:\t" + addr_to);
                return false;
            }

            if (tx->state == TX_STATE_ACCEPT && !wallet_from->try_apply_method(wallet_to, tx)) {
                DEBUG_COUT("block hash:\t" + bin2hex(block->get_block_hash()));
                DEBUG_COUT("tx hash:\t" + bin2hex(tx->hash));
                DEBUG_COUT("addr_from:\t" + addr_from);
                DEBUG_COUT("addr_to:\t" + addr_to);
                return false;
            }

            state_fee->add(fee + (tx->raw_tx.size() > 255 ? tx->raw_tx.size() - 255 : 0));
        }
    } else {
        DEBUG_COUT("block wrong type");
        return false;
    }

    return true;
}

bool BlockChain::can_apply_state_block(Block* block, bool check)
{
    wallet_map.get_wallet(ZERO_WALLET)->initialize(0, 0, "");

    auto common_block = dynamic_cast<CommonBlock*>(block);

    if (common_block) {
        {

#include <ctime>

            std::time_t now = common_block->get_block_timestamp();
            std::tm* ptm = std::localtime(&now);
            char buffer[32] = { 0 };
            // Format: Mo, 15.06.2009 20:20:00
            std::strftime(buffer, 32, "%a, %d.%m.%Y %H:%M:%S", ptm);

            DEBUG_COUT(buffer);
        }

        if (check) {
            for (auto tx : common_block->get_txs()) {
                const std::string& addr_to = tx->addr_to;
                Wallet* wallet_to = wallet_map.get_wallet(addr_to);

                if (!wallet_to) {
                    DEBUG_COUT("invalid wallet:\t" + addr_to);
                    continue;
                }

                uint64_t nonce = 0;
                uint64_t value;
                std::string data;

                std::tie(value, nonce, data) = wallet_to->serialize();

                if (tx->nonce != nonce) {
                    DEBUG_COUT("nonce not equal in state block");
                    DEBUG_COUT(addr_to);

                    return false;
                }

                if (tx->value != value) {
                    DEBUG_COUT("balance not equal in state block");
                    DEBUG_COUT(addr_to);
                    DEBUG_COUT(tx->value);
                    DEBUG_COUT(value);

                    return false;
                }
            }
        } else {
            for (auto tx : common_block->get_txs()) {
                const std::string& addr_to = tx->addr_to;
                Wallet* wallet_to = wallet_map.get_wallet(addr_to);

                if (!wallet_to) {
                    DEBUG_COUT("invalid wallet:\t" + addr_to);
                    continue;
                }

                wallet_to->initialize(tx->value, tx->nonce, std::string(tx->data));
            }
        }
    } else {
        DEBUG_COUT("block wrong type");
        return false;
    }

    return true;
}

bool BlockChain::can_apply_forging_block(Block* block)
{
    auto common_block = dynamic_cast<CommonBlock*>(block);

    if (common_block) {
        Wallet* state_fee = wallet_map.get_wallet(STATE_FEE_WALLET);
        uint64_t total_forging = 0;

        std::set<std::string> forging_nodes_add_trust;

        for (const auto* tx : common_block->get_txs()) {
            const std::string& addr_to = tx->addr_to;
            Wallet* wallet_to = wallet_map.get_wallet(addr_to);

            if (!wallet_to) {
                DEBUG_COUT("invalid wallet:\t" + addr_to);
                continue;
            }

            switch (tx->state) {
            case 0: {
            } break;
            case TX_STATE_FORGING_R: {
                wallet_to->add(tx->value);
                total_forging += tx->value;
            } break;
            case TX_STATE_FORGING_N: {
                forging_nodes_add_trust.insert(addr_to);
                wallet_to->add(tx->value);
                total_forging += tx->value;
            } break;
            case TX_STATE_FORGING_C: {
                wallet_to->add(tx->value);
                total_forging += tx->value;
            } break;
            case TX_STATE_FORGING_W: {
                wallet_to->add(tx->value);
                total_forging += tx->value;
            } break;
            case TX_STATE_FORGING_DAPP: {
                if (tx->data.size() != 25) {
                    DEBUG_COUT("wrong dapp addres");
                    return false;
                }

                const std::string& addr_from = bin2hex(tx->data);
                auto* wallet_from = dynamic_cast<DecentralizedApplication*>(wallet_map.get_wallet(addr_to));
                if (!wallet_from) {
                    DEBUG_COUT("wrong wallet type");
                    return false;
                }

                if (!wallet_from->sub(wallet_to, tx, 0)) {
                    DEBUG_COUT("not enough money on dapp wallet");
                    return false;
                }
            } break;
            default:
                DEBUG_COUT("wrong tx state\t" + std::to_string(tx->state));
                return false;
                break;
            }
        }

        if (total_forging <= FORGING_POOL + state_fee->get_value()) {
            state_fee->initialize((state_fee->get_value() + FORGING_POOL) - total_forging, 0, "");
        } else {
            if (total_forging > ((FORGING_POOL * 110) / 100 + state_fee->get_value())) {
                DEBUG_COUT("#############################################");
                DEBUG_COUT("############## POSSIBLE ERROR ###############");
                DEBUG_COUT("#############################################");
                DEBUG_COUT("Forged more than pool");
                DEBUG_COUT("total_forging");
                DEBUG_COUT(std::to_string(total_forging));
                DEBUG_COUT("FORGING_POOL");
                DEBUG_COUT(std::to_string(FORGING_POOL));
                DEBUG_COUT("state_fee");
                DEBUG_COUT(std::to_string(state_fee->get_value()));
                state_fee->initialize(0, 0, "");
            } else {
                state_fee->initialize(((FORGING_POOL * 110) / 100 + state_fee->get_value()) - total_forging, 0, "");
            }
        }

        for (auto& wallet_pair : wallet_map) {
            auto* wallet = dynamic_cast<CommonWallet*>(wallet_pair.second);
            if (!wallet) {
                DEBUG_COUT("invalid wallet type");
                DEBUG_COUT(wallet_pair.first);
                continue;
            }

            uint64_t w_state = wallet->get_state();

            if ((w_state & NODE_STATE_FLAG_TORRENT_FORGING) || (w_state & NODE_STATE_FLAG_PROXY_FORGING)) {
                if (forging_nodes_add_trust.find(wallet_pair.first) != forging_nodes_add_trust.end()) {
                    wallet->add_trust();
                } else {
                    wallet->sub_trust();
                }
            }
        }

        for (auto& wallet_pair : wallet_map) {
            auto* wallet = dynamic_cast<CommonWallet*>(wallet_pair.second);
            if (!wallet) {
                DEBUG_COUT("invalid wallet type");
                DEBUG_COUT(wallet_pair.first);
                continue;
            }
            wallet->apply_delegates();
        }

        {
            auto* father_of_wallets = dynamic_cast<CommonWallet*>(wallet_map.get_wallet(MASTER_WALLET_COIN_FORGING));
            auto* lookup_addreses = new std::deque<std::pair<std::string, uint64_t>>(father_of_wallets->get_delegated_from_list());

            while (true) {
                std::deque<std::pair<std::string, uint64_t>>* lookup_addreses_prev = wallet_request_addreses.load();
                if (wallet_request_addreses.compare_exchange_strong(lookup_addreses_prev, lookup_addreses)) {
                    delete lookup_addreses_prev;
                    break;
                }
            }
        }

        {
            auto* father_of_nodes = dynamic_cast<CommonWallet*>(wallet_map.get_wallet(MASTER_WALLET_NODE_FORGING));
            std::set<std::string> nodes;
            for (auto& delegate_pair : father_of_nodes->get_delegated_from_list()) {
                auto* wallet = dynamic_cast<CommonWallet*>(wallet_map.get_wallet(delegate_pair.first));
                if (!wallet) {
                    DEBUG_COUT("invalid wallet type");
                    DEBUG_COUT(delegate_pair.first);
                    continue;
                }

                uint64_t w_state = wallet->get_state();
                w_state |= NODE_STATE_FLAG_PRETEND;
                wallet->set_state(w_state);
                nodes.insert(delegate_pair.first);
            }
            for (auto& wallet_pair : wallet_map) {
                auto* wallet = dynamic_cast<CommonWallet*>(wallet_pair.second);
                if (!wallet) {
                    DEBUG_COUT("invalid wallet type");
                    DEBUG_COUT(wallet_pair.first);
                    continue;
                }

                uint64_t w_state = wallet->get_state();
                if (w_state & NODE_STATE_FLAG_PRETEND) {
                    if (nodes.find(wallet_pair.first) == nodes.end()) {
                        w_state &= ~NODE_STATE_FLAG_PRETEND;
                    }
                }
                uint64_t delegated_sum = wallet->get_delegated_from_sum();
                if (delegated_sum >= NODE_PROXY_SOFT_CAP) {
                    w_state |= NODE_STATE_FLAG_PROXY_SOFT_CAP;
                } else {
                    w_state &= ~NODE_STATE_FLAG_PROXY_SOFT_CAP;
                }
                if (delegated_sum >= NODE_TORRENT_SOFT_CAP) {
                    w_state |= NODE_STATE_FLAG_TORRENT_SOFT_CAP;
                } else {
                    w_state &= ~NODE_STATE_FLAG_TORRENT_SOFT_CAP;
                }
                {
                    w_state &= ~NODE_STATE_FLAG_TORRENT_SEED_CAP;
                    uint64_t seed_sum = 0;
                    for (auto& delegate_pair : wallet->get_delegated_from_list()) {
                        if (delegate_pair.first == wallet_pair.first) {
                            seed_sum += delegate_pair.second;
                        }
                    }
                    if (seed_sum >= NODE_TORRENT_SEED_CAP) {
                        w_state |= NODE_STATE_FLAG_TORRENT_SEED_CAP;
                    }
                }

                wallet->set_state(w_state);
            }
        }

        proxy_statistics.clear();

    } else {
        DEBUG_COUT("block wrong type");
        return false;
    }

    return true;
}

std::vector<RejectedTXInfo*>* BlockChain::make_rejected_tx_block(uint64_t)
{
    if (rejected_tx_list.empty()) {
        return nullptr;
    } else {
        auto new_list = new std::vector<RejectedTXInfo*>(rejected_tx_list);
        rejected_tx_list.clear();
        return new_list;
    }
}
