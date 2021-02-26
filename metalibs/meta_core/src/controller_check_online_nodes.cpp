#include "controller.hpp"
#include <meta_constants.hpp>
#include <meta_log.hpp>

#include <cmath>
#include <random>

namespace metahash::meta_core {

bool ControllerImplementation::check_online_nodes(uint64_t timestamp)
{
    auto current_generation = timestamp / CORE_LIST_RENEW_PERIOD;

    auto online_cores = cores.get_online_cores();

    if (online_cores.size() < METAHASH_PRIMARY_CORES_COUNT) {
        return false;
    }

    if (core_list_generation < current_generation) {
        if (timestamp == generation_check_timestamp) {
            return false;
        }
        generation_check_timestamp = timestamp;

        uint64_t accept_count = 0;
        if (current_generation - core_list_generation == 1) {
            accept_count = min_approve;
            if (std::find(current_cores.begin(), current_cores.end(), signer.get_mh_addr()) != std::end(current_cores)) {
                auto list = make_pretend_core_list(current_generation);
                if (!list.empty()) {
                    cores.send_no_return(RPC_CORE_LIST_APPROVE, list);
                }

                std::string string_list;
                string_list.insert(string_list.end(), list.begin(), list.end());
                proposed_cores[current_generation][string_list].insert(signer.get_mh_addr());
            }
        } else {
            auto list = make_pretend_core_list(current_generation);
            if (!list.empty()) {
                cores.send_no_return(RPC_CORE_LIST_APPROVE, list);
            }

            std::string string_list;
            string_list.insert(string_list.end(), list.begin(), list.end());
            proposed_cores[current_generation][string_list].insert(signer.get_mh_addr());

            {
                auto nodes = BC.get_node_state();
                for (auto&& [addr, roles] : nodes) {
                    if (roles.count(META_ROLE_CORE)) {
                        accept_count++;
                    }
                }
            }
            accept_count = std::ceil(accept_count * 51.0 / 100.0);
        }

        for (auto&& [cores_list, approve_cores] : proposed_cores[current_generation]) {
            if (approve_cores.size() >= accept_count) {
                auto primary_cores = crypto::split(cores_list, '\n');
                if (primary_cores.size() == METAHASH_PRIMARY_CORES_COUNT) {
                    bool all_here = true;
                    for (auto& check_core : primary_cores) {
                        if (!approve_cores.count(check_core)) {
                            DEBUG_COUT("no approve from\t" + check_core);
                            all_here = false;
                        }
                    }

                    if (all_here) {
                        current_cores = primary_cores;
                        core_list_generation = current_generation;

                        for (const auto& addr : current_cores) {
                            DEBUG_COUT(addr);
                        }

                        return true;
                    }
                }
            }
        }

        return false;
    }

    return true;
}

std::vector<char> ControllerImplementation::make_pretend_core_list(uint64_t current_generation)
{
    std::deque<std::string> cores_list;
    const auto nodes = BC.get_node_state();
    const auto online_cores = cores.get_online_cores();

    for (auto&& [addr, roles] : nodes) {
        if (online_cores.count(addr)
            && roles.count(META_ROLE_CORE)
            && abs(long(prev_timestamp) - long(core_last_block[addr])) < 3600) {
            cores_list.push_back(addr);
        }
    }

    std::string master;
    std::set<std::string> slaves;
    if (cores_list.size() >= METAHASH_PRIMARY_CORES_COUNT) {
        uint64_t last_block_hash_xx64 = crypto::get_xxhash64(blocks[last_applied_block]->get_data()) + current_generation;
        {
            std::sort(cores_list.begin(), cores_list.end());
            std::mt19937_64 r;
            r.seed(last_block_hash_xx64);
            std::shuffle(cores_list.begin(), cores_list.end(), r);
        }

        master = cores_list[0];
        for (uint i = 1; i < METAHASH_PRIMARY_CORES_COUNT; i++) {
            slaves.insert(cores_list[i]);
        }
    }

    std::vector<char> return_list;
    return_list.insert(return_list.end(), master.begin(), master.end());
    for (const auto& addr : slaves) {
        return_list.push_back('\n');
        return_list.insert(return_list.end(), addr.begin(), addr.end());
    }

    return return_list;
}

}