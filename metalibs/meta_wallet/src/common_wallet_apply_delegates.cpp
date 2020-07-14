#include <meta_constants.hpp>
#include <meta_wallet.h>

namespace metahash::meta_wallet {

void CommonWallet::apply_delegates()
{
    if (addition) {
        changed_wallets.push_back(this);

        addition->delegated_from_daly_snapshot = addition->delegated_from;
        addition->delegate_to_daly_snapshot = addition->delegate_to;

        if (addition->founder) {
            addition->limit += FOUNDER_DAILY_LIMIT_UP;
            return;
        }
        if (addition->state) {
            return;
        }
        if (addition->trust != 2) {
            return;
        }

        if (addition->delegated_from_sum) {
            return;
        }
        if (addition->delegated_to_sum) {
            return;
        }

        if (!addition->delegated_from.empty()) {
            return;
        }
        if (!addition->delegate_to.empty()) {
            return;
        }

        if (!addition->delegated_from_daly_snapshot.empty()) {
            return;
        }
        if (!addition->delegate_to_daly_snapshot.empty()) {
            return;
        }

        delete addition;
        addition = nullptr;
    }
}

}