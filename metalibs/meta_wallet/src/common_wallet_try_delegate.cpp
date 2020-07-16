#include <meta_constants.hpp>
#include <meta_log.hpp>
#include <meta_wallet.h>

namespace metahash::meta_wallet {

bool CommonWallet::try_delegate(Wallet* other, transaction::TX const* tx)
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

    changed_wallets.push_back(this);
    changed_wallets.push_back(wallet_to);
    return true;
}

}