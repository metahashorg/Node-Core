#include <meta_chain.h>

#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_chain {

std::pair<std::deque<std::string>, std::map<std::string, uint64_t>> BlockChain::make_forging_block_get_wallet_stats()
{
    std::map<std::string, std::pair<uint, uint>>* address_statistics;
    while (true) {
        std::map<std::string, std::pair<uint, uint>>* null_stat = nullptr;
        address_statistics = wallet_statistics.load();
        if (wallet_statistics.compare_exchange_strong(address_statistics, null_stat)) {
            break;
        }
    }

    std::deque<std::string> active_forging;
    std::map<std::string, uint64_t> pasive_forging;

    if (address_statistics) {
        for (const auto& addr_stat : *address_statistics) {
            if (addr_stat.second.first) {
                pasive_forging[addr_stat.first] = addr_stat.second.first;
            }
            if (addr_stat.second.second) {
                pasive_forging[addr_stat.first] += 1;

                for (uint i = 0; i < addr_stat.second.second; i++) {
                    active_forging.push_back(addr_stat.first);
                }
            }
        }
    } else {
        DEBUG_COUT("no statistics");
    }

    return { active_forging, pasive_forging };
}

}