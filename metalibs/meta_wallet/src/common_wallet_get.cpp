#include <meta_wallet.h>

namespace metahash::meta_wallet {

uint64_t CommonWallet::get_balance()
{
    if (addition) {
        return (balance - addition->delegated_to_sum);
    }
    return balance;
}

uint64_t CommonWallet::get_delegated_from_sum()
{
    if (addition) {
        return addition->delegated_from_sum;
    }
    return 0;
}

std::deque<std::pair<std::string, uint64_t>> CommonWallet::get_delegate_to_list()
{
    std::deque<std::pair<std::string, uint64_t>> return_list;
    if (addition) {
        for (const auto& delegate_to_pair : addition->delegate_to_daly_snapshot) {
            return_list.emplace_back(delegate_to_pair);
        }
    }

    return return_list;
}

std::deque<std::pair<std::string, uint64_t>> CommonWallet::get_delegated_from_list()
{
    std::deque<std::pair<std::string, uint64_t>> return_list;
    if (addition) {
        for (const auto& delegated_from_pair : addition->delegated_from_daly_snapshot) {
            return_list.emplace_back(delegated_from_pair);
        }
    }

    return return_list;
}

}