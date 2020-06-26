#include <set>

#include <chain.h>
#include <meta_log.hpp>
#include <open_ssl_decor.h>
#include <statics.hpp>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

namespace metahash::metachain {

BlockChain::BlockChain(boost::asio::io_context& io_context)
    : io_context(io_context)
{
}

bool BlockChain::can_apply_block(Block* block)
{
    return try_apply_block(block, false);
}

bool BlockChain::apply_block(Block* block)
{
    if (try_apply_block(block, true)) {
        prev_hash = block->get_block_hash();
        if (block->get_block_type() == BLOCK_TYPE_STATE) {
            state_hash_xx64 = crypto::get_xxhash64(block->get_data());
            {
#include <ctime>
                std::time_t now = block->get_block_timestamp();
                std::tm* ptm = std::localtime(&now);
                char buffer[32] = { 0 };
                // Format: Mo, 20.02.2002 20:20:02
                std::strftime(buffer, 32, "%a, %d.%m.%Y %H:%M:%S", ptm);

                DEBUG_COUT(buffer);
            }
            fill_node_state();
        }

        return true;
    }
    DEBUG_COUT("block is corrupt");
    wallet_map.clear_changes();
    return false;
}

std::atomic<std::map<std::string, std::pair<uint, uint>>*>& BlockChain::get_wallet_statistics()
{
    return wallet_statistics;
}

std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& BlockChain::get_wallet_request_addresses()
{
    return wallet_request_addreses;
}

std::set<std::string> BlockChain::check_addr(const std::string& addr)
{
    if (node_state.find(addr) == node_state.end()) {
        return std::set<std::string>();
    } else {
        return node_state[addr];
    }
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
        DEBUG_COUT("block is state or like\t" + crypto::bin2hex(block->get_block_hash()));
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

        for (const auto& tx : common_block->get_txs(io_context)) {
            if (tx.state == TX_STATE_FEE) {
                fee = tx.value;
                continue;
            }

            const std::string& addr_from = tx.addr_from;
            const std::string& addr_to = tx.addr_to;

            Wallet* wallet_to = wallet_map.get_wallet(addr_to);
            Wallet* wallet_from = wallet_map.get_wallet(addr_from);

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

bool BlockChain::can_apply_state_block(Block* block, bool check)
{
    wallet_map.get_wallet(ZERO_WALLET)->initialize(0, 0, "");

    auto common_block = dynamic_cast<CommonBlock*>(block);

    if (common_block) {

        if (check) {
            if (common_block->get_block_timestamp() >= 1572120000) {
                for (const auto& tx : common_block->get_txs(io_context)) {
                    const std::string& addr = tx.addr_to;
                    Wallet* wallet_to = wallet_map.get_wallet(addr);

                    if (!wallet_to) {
                        DEBUG_COUT("invalid wallet:\t" + addr);
                        continue;
                    }

                    if (auto* common_wallet = dynamic_cast<CommonWallet*>(wallet_to)) {
                        uint64_t nonce = 0;
                        uint64_t value;
                        std::string data;

                        std::tie(value, nonce, data) = common_wallet->serialize();

                        if (tx.nonce != nonce) {
                            DEBUG_COUT("nonce not equal in state block");
                            DEBUG_COUT(addr);

                            return false;
                        }

                        if (tx.value != value) {
                            DEBUG_COUT("balance not equal in state block");
                            DEBUG_COUT(addr);
                            DEBUG_COUT(tx.value);
                            DEBUG_COUT(value);

                            return false;
                        }

                        if (tx.data != data) {
                            DEBUG_COUT("data not equal in state block");
                            DEBUG_COUT(addr);
                            DEBUG_COUT(tx.data);
                            DEBUG_COUT(data);
                        }
                    }
                }
            } else {
                for (const auto& tx : common_block->get_txs(io_context)) {
                    const std::string& addr_to = tx.addr_to;
                    Wallet* wallet_to = wallet_map.get_wallet(addr_to);

                    if (!wallet_to) {
                        DEBUG_COUT("invalid wallet:\t" + addr_to);
                        continue;
                    }

                    uint64_t nonce = 0;
                    uint64_t value;
                    std::string data;

                    std::tie(value, nonce, data) = wallet_to->serialize();

                    if (tx.nonce != nonce) {
                        DEBUG_COUT("nonce not equal in state block");
                        DEBUG_COUT(addr_to);

                        return false;
                    }

                    if (tx.value != value) {
                        DEBUG_COUT("balance not equal in state block");
                        DEBUG_COUT(addr_to);
                        DEBUG_COUT(tx.value);
                        DEBUG_COUT(value);

                        return false;
                    }
                }
            }
        } else {
            for (const auto& tx : common_block->get_txs(io_context)) {
                const std::string& addr_to = tx.addr_to;
                Wallet* wallet_to = wallet_map.get_wallet(addr_to);

                if (!wallet_to) {
                    DEBUG_COUT("invalid wallet:\t" + addr_to);
                    continue;
                }

                if (auto common_wallet = dynamic_cast<CommonWallet*>(wallet_to)) {
                    common_wallet->initialize(tx.value, tx.nonce, std::string(tx.data));
                } else if (auto application = dynamic_cast<DecentralizedApplication*>(wallet_to)) {
                    application->initialize(tx.value, tx.nonce, std::string(tx.data));
                } else {
                    DEBUG_COUT("unknown wallet type wrong type");
                }
            }

            {
                auto* father_of_wallets = dynamic_cast<CommonWallet*>(wallet_map.get_wallet(MASTER_WALLET_COIN_FORGING));
                auto* lookup_addreses = new std::deque<std::pair<std::string, uint64_t>>(father_of_wallets->get_delegated_from_list());

                DEBUG_COUT("lookup_addreses.size() = \t" + std::to_string(lookup_addreses->size()));

                while (true) {
                    std::deque<std::pair<std::string, uint64_t>>* lookup_addreses_prev = wallet_request_addreses.load();
                    if (wallet_request_addreses.compare_exchange_strong(lookup_addreses_prev, lookup_addreses)) {
                        delete lookup_addreses_prev;
                        break;
                    }
                }
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
        uint64_t timestamp = common_block->get_block_timestamp();

        std::set<std::string> forging_nodes_add_trust;

        for (const auto& tx : common_block->get_txs(io_context)) {
            const std::string& addr_to = tx.addr_to;
            Wallet* wallet_to = wallet_map.get_wallet(addr_to);

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
                auto* f_wallet = dynamic_cast<CommonWallet*>(wallet_to);
                if (f_wallet) {
                    f_wallet->set_founder_limit();
                }
            } break;
            case TX_STATE_FORGING_DAPP: {
                if (tx.data.size() != 25) {
                    DEBUG_COUT("wrong dapp addres");
                    return false;
                }

                auto* wallet_from = dynamic_cast<DecentralizedApplication*>(wallet_map.get_wallet(addr_to));
                if (!wallet_from) {
                    DEBUG_COUT("wrong wallet type");
                    return false;
                }

                if (!wallet_from->sub(wallet_to, &tx, 0)) {
                    DEBUG_COUT("not enough money on dapp wallet");
                    return false;
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
            auto* wallet = dynamic_cast<CommonWallet*>(wallet_pair.second);
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
                w_state |= NODE_STATE_FLAG_PRETEND_COMMON;
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

void BlockChain::fill_node_state()
{
    std::unordered_map<std::string, std::set<std::string>, crypto::Hasher> states;

    for (auto&& [addr, p_wallet] : wallet_map) {
        auto* wallet = dynamic_cast<CommonWallet*>(p_wallet);
        if (!wallet) {
            DEBUG_COUT("invalid wallet type");
            DEBUG_COUT(addr);
            continue;
        }

        uint64_t w_state = wallet->get_state();

        for (auto&& [role, mask] : NODE_STATE_FLAG_FORGING) {
            if ((w_state & mask) == mask) {
                states[addr].insert(role);
            }
        }
    }

    uint64_t max_money = 0;
    std::string master_core_addr;
    for (auto&& [addr, roles]: states) {
        if (roles.find(META_ROLE_CORE) != roles.end()) {
            auto* wallet = dynamic_cast<CommonWallet*>(wallet_map.get_wallet(addr));
            auto&& [balance, state, data] = wallet->serialize();
            if (balance > max_money) {
                max_money = balance;
                master_core_addr = addr;
            }
        }
    }
    states[master_core_addr].insert(META_ROLE_MASTER);

    node_state.swap(states);
}
const std::unordered_map<std::string, std::set<std::string>, crypto::Hasher>& BlockChain::get_node_state()
{
    return node_state;
}

}