#include <meta_chain.h>

#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_chain {

void BlockChain::make_forging_block_node_reward(uint64_t timestamp, std::map<std::string, std::map<std::string, std::map<std::string, uint64_t>>>& type_geo_node_delegates, std::vector<char>& txs_buff)
{
    if (!type_geo_node_delegates.empty()) {
        auto state_fee = wallet_map.get_wallet(STATE_FEE_WALLET);

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
}

}