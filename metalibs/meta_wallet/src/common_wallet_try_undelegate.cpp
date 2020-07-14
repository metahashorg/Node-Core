#include <meta_log.hpp>
#include <meta_wallet.h>

namespace metahash::meta_wallet {

bool CommonWallet::try_undelegate(Wallet* other, transaction::TX const* tx)
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

    changed_wallets.push_back(this);
    changed_wallets.push_back(wallet_to);
    return true;
}

}