#include <meta_chain.h>

#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_chain {

bool BlockChain::can_apply_state_block(block::Block* block, bool check)
{
    wallet_map.get_wallet(ZERO_WALLET)->initialize(0, 0, "");

    auto common_block = dynamic_cast<block::CommonBlock*>(block);

    if (common_block) {

        if (check) {
            if (common_block->get_block_timestamp() >= 1572120000) {
                for (const auto& tx : common_block->get_txs(io_context)) {
                    const std::string& addr = tx.addr_to;
                    auto wallet_to = wallet_map.get_wallet(addr);

                    if (!wallet_to) {
                        DEBUG_COUT("invalid wallet:\t" + addr);
                        continue;
                    }

                    if (auto* common_wallet = dynamic_cast<meta_wallet::CommonWallet*>(wallet_to)) {
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
                    auto wallet_to = wallet_map.get_wallet(addr_to);

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
                auto wallet_to = wallet_map.get_wallet(addr_to);

                if (!wallet_to) {
                    DEBUG_COUT("invalid wallet:\t" + addr_to);
                    continue;
                }

                if (auto common_wallet = dynamic_cast<meta_wallet::CommonWallet*>(wallet_to)) {
                    common_wallet->initialize(tx.value, tx.nonce, std::string(tx.data));
                } else {
                    DEBUG_COUT("unknown wallet type wrong type");
                }
            }

            {
                auto* father_of_wallets = dynamic_cast<meta_wallet::CommonWallet*>(wallet_map.get_wallet(MASTER_WALLET_COIN_FORGING));
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

}