#include <meta_chain.h>

#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_chain {

bool BlockChain::can_apply_common_block(block::Block* block)
{

    auto common_block = dynamic_cast<block::CommonBlock*>(block);

    if (common_block) {
        uint64_t fee = 0;
        auto state_fee = wallet_map.get_wallet(STATE_FEE_WALLET);

        for (const auto& tx : common_block->get_txs(io_context)) {
            if (tx.state == TX_STATE_FEE) {
                fee = tx.value;
                continue;
            }

            const std::string& addr_from = tx.addr_from;
            const std::string& addr_to = tx.addr_to;

            auto wallet_to = wallet_map.get_wallet(addr_to);
            auto wallet_from = wallet_map.get_wallet(addr_from);

            if (!wallet_to || !wallet_from) {
                DEBUG_COUT("invalid wallet\t" + crypto::bin2hex(tx.hash));
                DEBUG_COUT(addr_from);
                DEBUG_COUT(addr_to);
                continue;
            }
            if (tx.state == TX_STATE_APPROVE) {
                wallet_from->sub(wallet_to, &tx, 0);
                continue;
            }

            if (tx.state == TX_STATE_TECH_NODE_STAT && tx.json_rpc && test_nodes.find(addr_from) != test_nodes.end()) {
                const auto& type = tx.json_rpc->parameters["type"];
                if (type == "Proxy"
                    || type == "InfrastructureTorrent"
                    || type == "Torrent"
                    || type == "Verifier") {

                    const auto& mhaddr = tx.json_rpc->parameters["address"];
                    node_statistics[type][mhaddr].count++;

                    if (tx.json_rpc->parameters["success"] != "false") {
                        uint64_t stat_value = 0;
                        if (type == "Proxy") {
                            try {
                                stat_value = std::stol(tx.json_rpc->parameters["rps"]);
                            } catch (...) {
                                stat_value = 0;
                            }
                        } else {
                            try {
                                stat_value = std::stol(tx.json_rpc->parameters["latency"]);
                            } catch (...) {
                                stat_value = 1'000'000;
                            }
                            stat_value = stat_value < 1'000'000 ? 1'000'000 - stat_value : 0;
                        }
                        node_statistics[type][mhaddr].stats[test_nodes.at(addr_from)].first += 1;
                        node_statistics[type][mhaddr].stats[test_nodes.at(addr_from)].second += stat_value;
                    }
                } else {
                    auto& mhaddr = tx.json_rpc->parameters["mhaddr"];
                    node_statistics["Proxy"][mhaddr].count++;

                    if (tx.json_rpc->parameters["success"] != "false") {
                        uint64_t rps = 0;
                        try {
                            rps = std::stol(tx.json_rpc->parameters["rps"]);
                        } catch (...) {
                            rps = 1;
                        }

                        if (rps > MINIMUM_PROXY_RPS && rps < (1000l * 1000l)) {
                            node_statistics["Proxy"][mhaddr].stats[test_nodes.at(addr_from)].first += 1;
                            node_statistics["Proxy"][mhaddr].stats[test_nodes.at(addr_from)].second += 1'000'000'000 / rps;
                        }
                    }
                }
                continue;
            }

            if (wallet_from->sub(wallet_to, &tx, fee + (tx.raw_tx.size() > 255 ? tx.raw_tx.size() - 255 : 0)) > 0) {
                DEBUG_COUT("tx hash:\t" + crypto::bin2hex(tx.hash));
                DEBUG_COUT("addr_from:\t" + addr_from);
                DEBUG_COUT("addr_to:\t" + addr_to);
                return false;
            }

            if (tx.state == TX_STATE_ACCEPT && !wallet_from->try_apply_method(wallet_to, &tx)) {
                DEBUG_COUT("block hash:\t" + crypto::bin2hex(block->get_block_hash()));
                DEBUG_COUT("tx hash:\t" + crypto::bin2hex(tx.hash));
                DEBUG_COUT("addr_from:\t" + addr_from);
                DEBUG_COUT("addr_to:\t" + addr_to);
                return false;
            }

            state_fee->add(fee + (tx.raw_tx.size() > 255 ? tx.raw_tx.size() - 255 : 0));
        }
    } else {
        DEBUG_COUT("block wrong type");
        return false;
    }

    return true;
}

}
