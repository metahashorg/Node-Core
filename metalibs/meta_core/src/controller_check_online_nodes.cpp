#include "controller.hpp"
#include <meta_constants.hpp>
#include <meta_log.hpp>

#include <cmath>
#include <random>

namespace metahash::meta_core {

bool ControllerImplementation::check_online_nodes(uint64_t timestamp)
{
    auto current_generation = timestamp / CORE_LIST_RENEW_PERIOD;

    if (core_list_generation < current_generation) {
        if (timestamp == generation_check_timestamp) {
            return false;
        }
        generation_check_timestamp = timestamp;

        make_online_core_list(current_generation);

        uint64_t registered_core_nodes_min_count = __UINT64_MAX__;
        {
            const auto nodes = BC.get_node_state();
            uint64_t registered_core_nodes_count = 0;
            for (auto&& [addr, roles] : nodes) {
                if (roles.count(META_ROLE_CORE)) {
                    registered_core_nodes_count++;
                }
            }
            registered_core_nodes_min_count = std::ceil(registered_core_nodes_count * 51.0 / 100.0);
        }

        auto online_cores = get_online_core_list(current_generation, registered_core_nodes_min_count);
        if (online_cores.size() < METAHASH_PRIMARY_CORES_COUNT) {
            return false;
        }

        {
            auto list = make_pretend_core_list(current_generation, online_cores);
            if (!list.empty()) {
                cores.send_no_return(RPC_CORE_LIST_APPROVE, list);
            }

            std::string string_list;
            string_list.insert(string_list.end(), list.begin(), list.end());
            proposed_cores[current_generation][string_list].insert(signer.get_mh_addr());
        }

        auto apply_approve_cores = [this, current_generation](const auto& cores_list, const auto& approve_cores) -> bool {
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

            return false;
        };

        for (auto&& [cores_list, approve_cores] : proposed_cores[current_generation]) {
            if (approve_cores.size() >= registered_core_nodes_min_count) {
                if (apply_approve_cores(cores_list, approve_cores)) {
                    return true;
                }
            }
        }

        for (auto&& [cores_list, approve_cores] : proposed_cores[current_generation]) {
            if (approve_cores.size() >= METAHASH_PRIMARY_CORES_COUNT) {
                if (apply_approve_cores(cores_list, approve_cores)) {
                    return true;
                }
            }
        }

        return false;
    }

    return true;
}

void ControllerImplementation::make_online_core_list(uint64_t current_generation)
{
    const auto nodes = BC.get_node_state();
    const auto online_cores = cores.get_online_cores();

    std::vector<char> core_list;
    bool first = true;
    for (auto&& [addr, roles] : nodes) {
        if (online_cores.count(addr)
            && roles.count(META_ROLE_CORE)
            && abs(long(prev_timestamp) - long(core_last_block[addr])) < 3600) {

            if (first) {
                first = false;
            } else {
                core_list.push_back('\n');            
            }

            core_list.insert(core_list.end(), addr.begin(), addr.end());
            online_sync_cores[current_generation][signer.get_mh_addr()].insert(addr);
        }
    }

    if (!core_list.empty()) {
        cores.send_no_return(RPC_CORE_LIST_ONLINE, core_list);
    }
}

std::set<std::string> ControllerImplementation::get_online_core_list(uint64_t current_generation, uint64_t registered_core_nodes_min_count)
{
    std::set<std::string> cores_list;

    auto count_cores_in_generation = [this, registered_core_nodes_min_count, &cores_list](uint64_t checked_generation) {
        std::map<std::string, uint> core_online_count;
        auto cores_with_count_more_than = [this, &core_online_count, &cores_list](uint64_t min_count) {
            for (auto&& [online_core, count] : core_online_count) {
                if (count >= min_count) {
                    cores_list.insert(online_core);
                }
            }
        };

        for (auto&& [send_core, core_list] : online_sync_cores[checked_generation]) {
            for (auto&& online_core : core_list) {
                core_online_count[online_core]++;
            }
        }

        cores_list = online_sync_cores[checked_generation].begin()->second;
        for (auto&& [send_core, core_list] : online_sync_cores[checked_generation]) {
            std::set<std::string> temp_set;
            std::set_intersection(cores_list.begin(), cores_list.end(), core_list.begin(), core_list.end(), std::inserter(temp_set, temp_set.begin()));
            cores_list.swap(temp_set);
        }

        if (cores_list.size() < registered_core_nodes_min_count) {
            cores_with_count_more_than(registered_core_nodes_min_count);
        }

        if (cores_list.size() < METAHASH_PRIMARY_CORES_COUNT) {
            cores_with_count_more_than(METAHASH_PRIMARY_CORES_COUNT);
        }
    };

    if (online_sync_cores[current_generation].size() < METAHASH_PRIMARY_CORES_COUNT) {
        return std::set<std::string>();
    }

    count_cores_in_generation(current_generation);

    if (cores_list.size() < METAHASH_PRIMARY_CORES_COUNT) {
        return std::set<std::string>();
    }

    return cores_list;
}

std::vector<char> ControllerImplementation::make_pretend_core_list(uint64_t current_generation, const std::set<std::string>& cores_list)
{
    std::deque<std::string> cores_deq;
    cores_deq.insert(cores_deq.end(), cores_list.begin(), cores_list.end());

    std::string master;
    std::set<std::string> slaves;
    if (cores_deq.size() >= METAHASH_PRIMARY_CORES_COUNT) {
        uint64_t last_block_hash_xx64 = crypto::get_xxhash64(blocks[last_applied_block]->get_data()) + current_generation;
        {
            std::mt19937_64 r;
            r.seed(last_block_hash_xx64);
            std::shuffle(cores_deq.begin(), cores_deq.end(), r);
        }

        master = cores_deq[0];
        for (uint i = 1; i < METAHASH_PRIMARY_CORES_COUNT; i++) {
            slaves.insert(cores_deq[i]);
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