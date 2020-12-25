#include <meta_chain.h>

#include <meta_constants.hpp>
#include <meta_log.hpp>

#include <random>

namespace metahash::meta_chain {

void BlockChain::make_forging_block_random_reward(uint64_t timestamp, std::deque<std::string>& active_forging, std::vector<char>& txs_buff)
{
    auto state_fee = wallet_map.get_wallet(STATE_FEE_WALLET);

    if (!active_forging.empty()) {
        {
            DEBUG_COUT("state_hash_xx64\t" + std::to_string(state_hash_xx64));
            std::mt19937_64 r;
            r.seed(state_hash_xx64);
            std::shuffle(active_forging.begin(), active_forging.end(), r);
        }

        for (uint i = 0; i < 1000; i++) {
            if (active_forging.size() > i) {
                auto addr = active_forging[i];
                for (auto addr_it = active_forging.begin() + i + 1; addr_it != active_forging.end();) {
                    if (*addr_it == addr) {
                        addr_it = active_forging.erase(addr_it);
                    } else {
                        addr_it++;
                    }
                }
            }
        }

        const uint64_t forging_count_total = ((FORGING_POOL(timestamp) + state_fee->get_value()) * 1) / 10;

        const uint64_t FORGING_RANDOM_REWARD_1 = forging_count_total * 4 / 10;
        const uint64_t FORGING_RANDOM_REWARD_2 = forging_count_total * 1 / 10;
        const uint64_t FORGING_RANDOM_REWARD_3 = forging_count_total * 5 / 100;
        const uint64_t FORGING_RANDOM_REWARD_4 = forging_count_total * 415 / 10000;
        const uint64_t FORGING_RANDOM_REWARD_5 = forging_count_total * 335 / 10000;

        const uint64_t FORGING_RANDOM_REWARD_6_100 = forging_count_total * 1 / 1000;
        const uint64_t FORGING_RANDOM_REWARD_101_1000 = forging_count_total * 2 / 10000;

        uint64_t reward_bank = 0;
        if (active_forging.size() < 1000) {
            if (active_forging.size() >= 100) {
                reward_bank += FORGING_RANDOM_REWARD_101_1000 * (1000 - active_forging.size());
            } else {
                reward_bank += FORGING_RANDOM_REWARD_101_1000 * (1000 - 100);
                if (active_forging.size() >= 6) {
                    reward_bank += FORGING_RANDOM_REWARD_6_100 * (100 - active_forging.size());
                } else {
                    reward_bank += FORGING_RANDOM_REWARD_6_100 * (100 - 6);

                    if (active_forging.size() < 5) {
                        reward_bank += FORGING_RANDOM_REWARD_5;
                    }
                    if (active_forging.size() < 4) {
                        reward_bank += FORGING_RANDOM_REWARD_4;
                    }
                    if (active_forging.size() < 3) {
                        reward_bank += FORGING_RANDOM_REWARD_3;
                    }
                    if (active_forging.size() < 2) {
                        reward_bank += FORGING_RANDOM_REWARD_2;
                    }
                }
            }
        }

        uint64_t reward_bank_per_one = reward_bank / active_forging.size();
        for (uint i = 0; i < 1 && i < active_forging.size(); i++) {
            auto&& state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_1 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

            crypto::append_varint(txs_buff, state_tx.size());
            txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
        }
        for (uint i = 1; i < 2 && i < active_forging.size(); i++) {
            auto&& state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_2 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

            crypto::append_varint(txs_buff, state_tx.size());
            txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
        }
        for (uint i = 2; i < 3 && i < active_forging.size(); i++) {
            auto&& state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_3 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

            crypto::append_varint(txs_buff, state_tx.size());
            txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
        }
        for (uint i = 3; i < 4 && i < active_forging.size(); i++) {
            auto&& state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_4 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

            crypto::append_varint(txs_buff, state_tx.size());
            txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
        }
        for (uint i = 4; i < 5 && i < active_forging.size(); i++) {
            auto&& state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_5 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

            crypto::append_varint(txs_buff, state_tx.size());
            txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
        }
        for (uint i = 5; i < 100 && i < active_forging.size(); i++) {
            auto&& state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_6_100 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

            crypto::append_varint(txs_buff, state_tx.size());
            txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
        }
        for (uint i = 100; i < 1000 && i < active_forging.size(); i++) {
            auto&& state_tx = make_forging_tx(active_forging[i], FORGING_RANDOM_REWARD_101_1000 + reward_bank_per_one, {}, TX_STATE_FORGING_R);

            crypto::append_varint(txs_buff, state_tx.size());
            txs_buff.insert(txs_buff.end(), state_tx.begin(), state_tx.end());
        }
    }
}

}