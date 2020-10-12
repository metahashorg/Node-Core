#include <meta_chain.h>

#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_chain {

bool BlockChain::can_apply_forging_block(block::Block* block)
{
    auto common_block = dynamic_cast<block::CommonBlock*>(block);

    if (common_block) {
        auto state_fee = wallet_map.get_wallet(STATE_FEE_WALLET);
        uint64_t total_forging = 0;
        uint64_t timestamp = common_block->get_block_timestamp();

        std::set<std::string> forging_nodes_add_trust;

        for (const auto& tx : common_block->get_txs(io_context)) {
            const std::string& addr_to = tx.addr_to;
            auto wallet_to = wallet_map.get_wallet(addr_to);

            if (!wallet_to) {
                DEBUG_COUT("invalid wallet:\t" + addr_to);
                continue;
            }

            switch (tx.state) {
            case 0: {
            } break;
            case TX_STATE_FORGING_R:
            case TX_STATE_FORGING_C:
            case TX_STATE_FORGING_W: {
                wallet_to->add(tx.value);
                total_forging += tx.value;
            } break;
            case TX_STATE_FORGING_N: {
                forging_nodes_add_trust.insert(addr_to);
                wallet_to->add(tx.value);
                total_forging += tx.value;
            } break;
            case TX_STATE_FORGING_TEAM: {
                wallet_to->add(tx.value);
            } break;
            case TX_STATE_FORGING_FOUNDER: {
                wallet_to->add(tx.value);
                auto* f_wallet = dynamic_cast<meta_wallet::CommonWallet*>(wallet_to);
                if (f_wallet) {
                    f_wallet->set_founder_limit();
                }
            } break;
            default:
                DEBUG_COUT("wrong tx state\t" + std::to_string(tx.state));
                return false;
                break;
            }
        }

        if (total_forging <= FORGING_POOL(timestamp) + state_fee->get_value()) {
            state_fee->initialize((state_fee->get_value() + FORGING_POOL(timestamp)) - total_forging, 0, "");
        } else {
            if (total_forging > ((FORGING_POOL(timestamp) * 110) / 100 + state_fee->get_value())) {
                DEBUG_COUT("#############################################");
                DEBUG_COUT("############## POSSIBLE ERROR ###############");
                DEBUG_COUT("#############################################");
                DEBUG_COUT("Forged more than pool");
                DEBUG_COUT("total_forging");
                DEBUG_COUT(std::to_string(total_forging));
                DEBUG_COUT("FORGING_POOL");
                DEBUG_COUT(std::to_string(FORGING_POOL(timestamp)));
                DEBUG_COUT("state_fee");
                DEBUG_COUT(std::to_string(state_fee->get_value()));
                state_fee->initialize(0, 0, "");
            } else {
                state_fee->initialize(((FORGING_POOL(timestamp) * 110) / 100 + state_fee->get_value()) - total_forging, 0, "");
            }
        }

        for (auto& wallet_pair : wallet_map) {
            auto* wallet = dynamic_cast<meta_wallet::CommonWallet*>(wallet_pair.second);
            if (!wallet) {
                DEBUG_COUT("invalid wallet type");
                DEBUG_COUT(wallet_pair.first);
                continue;
            }

            uint64_t w_state = wallet->get_state();

            bool doing_forging = false;
            for (auto&& [role, mask] : NODE_STATE_FLAG_FORGING) {
                if ((w_state & mask) == mask) {
                    doing_forging = true;
                }
            }

            if (doing_forging) {
                if (forging_nodes_add_trust.find(wallet_pair.first) != forging_nodes_add_trust.end()) {
                    wallet->add_trust();
                } else {
                    wallet->sub_trust();
                }
            }
        }

        for (auto& wallet_pair : wallet_map) {
            auto* wallet = dynamic_cast<meta_wallet::CommonWallet*>(wallet_pair.second);
            if (!wallet) {
                DEBUG_COUT("invalid wallet type");
                DEBUG_COUT(wallet_pair.first);
                continue;
            }
            wallet->apply_delegates();
        }

        {
            auto* father_of_wallets = dynamic_cast<meta_wallet::CommonWallet*>(wallet_map.get_wallet(MASTER_WALLET_COIN_FORGING));
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
            auto&& check_caps = [](uint64_t& state, const auto seed_sum, const auto delegated_sum, const std::string& role) {
                if (state & NODE_STATE_FLAG_PRETEND.at(role)) {
                    if (delegated_sum >= NODE_SOFT_CAP.at(role)) {
                        state |= NODE_STATE_FLAG_SOFT_CAP.at(role);
                    } else {
                        state &= ~NODE_STATE_FLAG_SOFT_CAP.at(role);
                    }
                    if (seed_sum >= NODE_SEED_CAP.at(role)) {
                        state |= NODE_STATE_FLAG_SEED_CAP.at(role);
                    } else {
                        state &= ~NODE_STATE_FLAG_SEED_CAP.at(role);
                    }
                } else {
                    state &= ~NODE_STATE_FLAG_SOFT_CAP.at(role);
                    state &= ~NODE_STATE_FLAG_SEED_CAP.at(role);
                }
            };

            auto* father_of_nodes = dynamic_cast<meta_wallet::CommonWallet*>(wallet_map.get_wallet(MASTER_WALLET_NODE_FORGING));
            std::set<std::string> nodes;
            for (auto& delegate_pair : father_of_nodes->get_delegated_from_list()) {
                auto* wallet = dynamic_cast<meta_wallet::CommonWallet*>(wallet_map.get_wallet(delegate_pair.first));
                if (!wallet) {
                    DEBUG_COUT("invalid wallet type");
                    DEBUG_COUT(delegate_pair.first);
                    continue;
                }

                uint64_t w_state = wallet->get_state();
                w_state |= NODE_STATE_FLAG_PRETEND_COMMON;
                wallet->set_state(w_state);
                nodes.insert(delegate_pair.first);
            }
            for (auto& wallet_pair : wallet_map) {
                auto* wallet = dynamic_cast<meta_wallet::CommonWallet*>(wallet_pair.second);
                if (!wallet) {
                    DEBUG_COUT("invalid wallet type");
                    DEBUG_COUT(wallet_pair.first);
                    continue;
                }

                uint64_t w_state = wallet->get_state();
                if (w_state & NODE_STATE_FLAG_PRETEND_COMMON) {
                    if (nodes.find(wallet_pair.first) == nodes.end()) {
                        w_state &= ~NODE_STATE_FLAG_PRETEND_COMMON;
                    }
                }

                if (w_state & NODE_STATE_FLAG_PRETEND_COMMON) {
                    uint64_t delegated_sum = wallet->get_delegated_from_sum();

                    uint64_t seed_sum = 0;
                    for (auto& delegate_pair : wallet->get_delegated_from_list()) {
                        if (delegate_pair.first == wallet_pair.first) {
                            seed_sum += delegate_pair.second;
                        }
                    }

                    for (auto&& role : ROLES) {
                        check_caps(w_state, seed_sum, delegated_sum, role);
                    }
                }

                wallet->set_state(w_state);
            }
        }

        node_statistics.clear();

    } else {
        DEBUG_COUT("block wrong type");
        return false;
    }

    return true;
}

}