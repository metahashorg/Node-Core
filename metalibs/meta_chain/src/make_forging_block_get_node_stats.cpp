#include <meta_chain.h>

#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_chain {

std::pair<std::map<std::string, std::map<std::string, std::map<std::string, uint64_t>>>, std::map<std::string, uint64_t>> BlockChain::make_forging_block_get_node_stats()
{
    std::map<std::string, uint64_t> delegates;
    //          ROLE                   GEO                  NODE     DELEGATED
    std::map<std::string, std::map<std::string, std::map<std::string, uint64_t>>> type_geo_node_delegates;

    std::set<std::string> revard_nodes;
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

    return { type_geo_node_delegates, delegates };
}

}