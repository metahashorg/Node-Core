#include <meta_chain.h>

#include <meta_log.hpp>
#include <meta_constants.hpp>

#include <random>

namespace metahash::meta_chain {

uint64_t BlockChain::FORGING_POOL(uint64_t ts)
{
    uint64_t return_pool = 0;
    for (const auto [start_ts, pool] : FORGING_POOL_PER_YEAR) {
        if (ts > start_ts) {
            return_pool = pool;
        } else {
            break;
        }
    }
    return return_pool;
}

std::vector<char> make_forging_tx(const std::string& address, uint64_t reward, const std::vector<unsigned char>& data, uint64_t tx_type)
{
    std::vector<char> forging_tx;
    auto bin_address = crypto::hex2bin(address);

    forging_tx.insert(forging_tx.end(), bin_address.begin(), bin_address.end());

    crypto::append_varint(forging_tx, reward);
    crypto::append_varint(forging_tx, 0);
    crypto::append_varint(forging_tx, 0);

    crypto::append_varint(forging_tx, data.size());
    forging_tx.insert(forging_tx.end(), data.begin(), data.end());

    crypto::append_varint(forging_tx, 0);
    crypto::append_varint(forging_tx, 0);

    crypto::append_varint(forging_tx, tx_type);

    return forging_tx;
}

block::Block* BlockChain::make_forging_block(uint64_t timestamp)
{
    uint64_t block_type = BLOCK_TYPE_FORGING;
    std::vector<char> txs_buff;

    auto state_fee = wallet_map.get_wallet(STATE_FEE_WALLET);

    {
        std::map<std::string, uint64_t> delegates;
        //          ROLE                   GEO                  NODE     DELEGATED
        std::map<std::string, std::map<std::string, std::map<std::string, uint64_t>>> type_geo_node_delegates;
        std::set<std::string> revard_nodes;

        /*{
            std::map<std::string, std::string> msgs;
            std::vector<char> addr_msg;

            for (auto&& [type, nodes] : node_statistics) {
                for (auto&& [addr, node_stat] : nodes) {
                    // if (node_stat.count) {
                    auto&& str_count = std::to_string(node_stat.count);

                    addr_msg.clear();
                    addr_msg.push_back('\"');
                    addr_msg.insert(addr_msg.end(), addr.begin(), addr.end());
                    addr_msg.push_back(';');
                    addr_msg.insert(addr_msg.end(), str_count.begin(), str_count.end());

                    for (auto&& [geo_name, geo_stat] : node_stat.stats) {
                        auto&& str_cnt = std::to_string(geo_stat.first);
                        auto&& str_sum = std::to_string(geo_stat.second);

                        addr_msg.push_back(';');
                        addr_msg.insert(addr_msg.end(), geo_name.begin(), geo_name.end());
                        addr_msg.push_back(';');
                        addr_msg.insert(addr_msg.end(), str_cnt.begin(), str_cnt.end());
                        addr_msg.push_back(';');
                        addr_msg.insert(addr_msg.end(), str_sum.begin(), str_sum.end());
                    }

                    msgs[type].insert(msgs[type].end(), addr_msg.begin(), addr_msg.end());
                    addr_msg.clear();
                    // }
                }
            }
            for (auto&& [type, info] : msgs) {
                DEBUG_COUT(type);
                DEBUG_COUT(info);
            }
        }*/

        {

            //TODO Add Cores
            for (auto&& [type, nodes] : node_statistics) {
                if (ROLES.find(type) == ROLES.end()) {
                    DEBUG_COUT("Unknown role:\t" + type);
                    continue;
                }
                for (auto&& [addr, node_stat] : nodes) {
                    auto* wallet = dynamic_cast<meta_wallet::CommonWallet*>(wallet_map.get_wallet(addr));
                    if (wallet) {
                        const auto w_state = wallet->get_state();
                        const auto state_mask = NODE_STATE_FLAG_FORGING.at(type);

                        if ((w_state & state_mask) == state_mask) {
                            std::string geo;
                            uint64_t success_size = 0;
                            uint64_t average = 0;

                            for (auto&& [geo_name, geo_stat] : node_stat.stats) {

                                success_size += geo_stat.second;
                                uint64_t geo_average = geo_stat.second / geo_stat.first;

                                if (geo_average > average) {
                                    average = geo_average;
                                    geo = geo_name;
                                }
                            }

                            uint64_t min_for_reward = node_stat.count * 95 / 100;
                            if (success_size > min_for_reward && average >= MINIMUM_AVERAGE_PROXY_RPS && revard_nodes.insert(addr).second) {
                                uint64_t node_total_delegate = 0;
                                for (auto&& [d_addr, d_value] : wallet->get_delegated_from_list()) {

                                    uint64_t node_wallet_delegate;
                                    if (node_total_delegate + d_value > NODE_HARD_CAP.at(type)) {
                                        node_wallet_delegate = NODE_HARD_CAP.at(type) - node_total_delegate;
                                    } else {
                                        node_wallet_delegate = d_value;
                                    }

                                    delegates[d_addr] += node_wallet_delegate;

                                    node_total_delegate += node_wallet_delegate;
                                    if (node_total_delegate >= NODE_HARD_CAP.at(type)) {
                                        break;
                                    }
                                }

                                if (node_total_delegate <= NODE_HARD_CAP.at(type)) {
                                    type_geo_node_delegates[type][geo][addr] = node_total_delegate;
                                } else {
                                    type_geo_node_delegates[type][geo][addr] = NODE_HARD_CAP.at(type);
                                }

                            } else {
                                DEBUG_COUT("not enough tests for " + type + ":\t" + addr + "\t" + std::to_string(min_for_reward) + "\t" + std::to_string(success_size));
                            }
                        }
                    } else {
                        DEBUG_COUT("Invalid wallet:\t" + addr);
                    }
                }
            }
        }

        if (!type_geo_node_delegates.empty()) {
            std::set<std::string> reward_nodes;
            static const std::vector<std::string> types_by_value {
                "Proxy",
                "InfrastructureTorrent",
                "Torrent",
                "Verifier"
                //TODO Core
                // ,"Core"
            };

            uint64_t forging_node_units = 0;
            std::map<std::string, std::map<std::string, uint64_t>> geo_total_units = {
                { "Proxy",
                    { { "us", 0 },
                        { "eu", 0 },
                        { "cn", 0 } } },
                { "InfrastructureTorrent",
                    { { "us", 0 },
                        { "eu", 0 },
                        { "cn", 0 } } },
                { "Torrent",
                    { { "us", 0 },
                        { "eu", 0 },
                        { "cn", 0 } } },
                { "Verifier",
                    { { "us", 0 },
                        { "eu", 0 },
                        { "cn", 0 } } }
                //TODO Core
                // ,{ "Core",
                //     { { "us", 0 }}}
            };

            for (const auto& type : types_by_value) {
                for (auto&& [geo_name, geo_nodes] : type_geo_node_delegates[type]) {
                    for (auto&& [node_name, node_delegate] : geo_nodes) {
                        geo_total_units[type][geo_name] += node_delegate;
                        forging_node_units += node_delegate;
                    }
                }
            }

            const uint64_t pool = (FORGING_POOL(timestamp) + state_fee->get_value());
            //TODO Core
            // uint64_t forging_node_total = (pool * 10) / 100;
            uint64_t forging_node_total = (pool * 16) / 100;

            std::map<std::string, std::map<std::string, uint64_t>> geo_total = {
                { "Proxy",
                    { { "us", (pool * 2) / 100 },
                        { "eu", (pool * 2) / 100 },
                        { "cn", (pool * 2) / 100 } } },
                { "InfrastructureTorrent",
                    { { "us", (pool * 2) / 100 },
                        { "eu", (pool * 2) / 100 },
                        { "cn", (pool * 2) / 100 } } },
                { "Torrent",
                    { { "us", (pool * 2) / 100 },
                        { "eu", (pool * 2) / 100 },
                        { "cn", (pool * 2) / 100 } } },
                { "Verifier",
                    { { "us", (pool * 2) / 100 },
                        { "eu", (pool * 2) / 100 },
                        { "cn", (pool * 2) / 100 } } }
                //TODO Core
                // ,{ "Core",
                //     { { "us", (pool * 6) / 100 } } }
            };

            const double forging_node_per_unit = double(forging_node_total) / double(forging_node_units);
            const std::map<std::string, std::map<std::string, double>> geo_per_unit = {
                { "Proxy",
                    { { "us", double(geo_total["Proxy"]["us"]) / double(geo_total_units["Proxy"]["us"]) },
                        { "eu", double(geo_total["Proxy"]["eu"]) / double(geo_total_units["Proxy"]["eu"]) },
                        { "cn", double(geo_total["Proxy"]["cn"]) / double(geo_total_units["Proxy"]["cn"]) } } },
                { "InfrastructureTorrent",
                    { { "us", double(geo_total["InfrastructureTorrent"]["us"]) / double(geo_total_units["InfrastructureTorrent"]["us"]) },
                        { "eu", double(geo_total["InfrastructureTorrent"]["eu"]) / double(geo_total_units["InfrastructureTorrent"]["eu"]) },
                        { "cn", double(geo_total["InfrastructureTorrent"]["cn"]) / double(geo_total_units["InfrastructureTorrent"]["cn"]) } } },
                { "Torrent",
                    { { "us", double(geo_total["Torrent"]["us"]) / double(geo_total_units["Torrent"]["us"]) },
                        { "eu", double(geo_total["Torrent"]["eu"]) / double(geo_total_units["Torrent"]["eu"]) },
                        { "cn", double(geo_total["Torrent"]["cn"]) / double(geo_total_units["Torrent"]["cn"]) } } },
                { "Verifier",
                    { { "us", double(geo_total["Verifier"]["us"]) / double(geo_total_units["Verifier"]["us"]) },
                        { "eu", double(geo_total["Verifier"]["eu"]) / double(geo_total_units["Verifier"]["eu"]) },
                        { "cn", double(geo_total["Verifier"]["cn"]) / double(geo_total_units["Verifier"]["cn"]) } } }
            };

            for (auto&& [node_role, role_stat] : type_geo_node_delegates) {
                for (auto&& [geo_name, geo_nodes] : role_stat) {
                    for (auto&& [node_name, node_delegate] : geo_nodes) {

                        auto forging_node_reward = uint64_t(double(node_delegate) * forging_node_per_unit);
                        auto forging_node_reward_geo = uint64_t(double(node_delegate) * geo_per_unit.at(node_role).at(geo_name));

                        DEBUG_COUT("NODE_FORGING:\t" + node_name + "\t" + node_role + "\t" + geo_name + "\t" + std::to_string(forging_node_reward) + "\t" + std::to_string(forging_node_reward_geo));

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

                        auto&& state_tx = make_forging_tx(node_name, forging_node_reward + forging_node_reward_geo, {}, TX_STATE_FORGING_N);

                        if (!state_tx.empty()) {
                            crypto::append_varint(txs_buff, state_tx.size());
                            txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
                        }
                    }
                }
            }
        }

        uint64_t forging_coin_units = 0;
        for (auto& delegate_pair : delegates) {
            forging_coin_units += delegate_pair.second;
        }

        if (forging_coin_units) {
            uint64_t forging_coin_total = ((FORGING_POOL(timestamp) + state_fee->get_value()) * 5) / 10;
            double forging_coin_per_one = double(forging_coin_total) / double(forging_coin_units);

            for (auto& delegate_pair : delegates) {
                std::string coin_addres = delegate_pair.first;
                auto forging_coin = uint64_t(forging_coin_per_one * double(delegate_pair.second));

                if (forging_coin_total < forging_coin) {
                    continue;
                }

                forging_coin_total -= forging_coin;

                auto&& state_tx = make_forging_tx(coin_addres, forging_coin, {}, TX_STATE_FORGING_C);

                if (!state_tx.empty()) {
                    crypto::append_varint(txs_buff, state_tx.size());
                    txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
                }
            }
        }
    }

    {
        std::map<std::string, std::pair<uint, uint>>* address_statistics;
        while (true) {
            std::map<std::string, std::pair<uint, uint>>* null_stat = nullptr;
            address_statistics = wallet_statistics.load();
            if (wallet_statistics.compare_exchange_strong(address_statistics, null_stat)) {
                break;
            }
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
        } else {
            DEBUG_COUT("no statistics");
        }

        if (!pasive_forging.empty()) {
            uint64_t forging_shares_total = 0;
            for (const auto& addr_pair : pasive_forging) {
                forging_shares_total += addr_pair.second;
            }

            const uint64_t forging_count_total = ((FORGING_POOL(timestamp) + state_fee->get_value()) * 1) / 10;

            const uint64_t FORGING_PASSIVE_REWARD = forging_count_total / 10;

            uint64_t reward_passive_per_share = FORGING_PASSIVE_REWARD / forging_shares_total;
            for (const auto& addr_pair : pasive_forging) {
                auto&& state_tx = make_forging_tx(addr_pair.first, addr_pair.second * reward_passive_per_share, {}, TX_STATE_FORGING_W);
                crypto::append_varint(txs_buff, state_tx.size());
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

            const uint64_t forging_count_total = ((FORGING_POOL(timestamp) + state_fee->get_value()) * 1) / 10;

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
                auto&& state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_1 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

                crypto::append_varint(txs_buff, state_tx.size());
                txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
            }
            for (uint i = 1; i < 2 && i < active_forging.size(); i++) {
                auto&& state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_2 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

                crypto::append_varint(txs_buff, state_tx.size());
                txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
            }
            for (uint i = 2; i < 3 && i < active_forging.size(); i++) {
                auto&& state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_3 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

                crypto::append_varint(txs_buff, state_tx.size());
                txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
            }
            for (uint i = 3; i < 4 && i < active_forging.size(); i++) {
                auto&& state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_4 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

                crypto::append_varint(txs_buff, state_tx.size());
                txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
            }
            for (uint i = 4; i < 5 && i < active_forging.size(); i++) {
                auto&& state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_5 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

                crypto::append_varint(txs_buff, state_tx.size());
                txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
            }
            for (uint i = 5; i < 100 && i < active_forging.size(); i++) {
                auto&& state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_6_100 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

                crypto::append_varint(txs_buff, state_tx.size());
                txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
            }
            for (uint i = 100; i < 1000 && i < active_forging.size(); i++) {
                auto&& state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_101_1000 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

                crypto::append_varint(txs_buff, state_tx.size());
                txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
            }
        }
    }

    {
        auto&& state_tx = make_forging_tx(TEAM_WALLET, FORGING_TEAM_REWARD, {}, TX_STATE_FORGING_TEAM);

        crypto::append_varint(txs_buff, state_tx.size());
        txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
    }

    txs_buff.push_back(0);

    return make_block(block_type, timestamp, prev_hash, txs_buff);
}

}