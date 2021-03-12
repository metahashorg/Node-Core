#include <meta_constants.hpp>
#include <meta_log.hpp>

#include "controller.hpp"

#include <random>

namespace metahash::meta_core {

void ControllerImplementation::check_if_chain_actual()
{
    auto index = actualization_iteration % 2;
    not_actualized[index] = 0;
    actualization_iteration++;

    cores.send_with_callback(RPC_LAST_BLOCK, std::vector<char>(), [this, index](const std::string& mh_addr, const std::vector<char>& resp) {
        if (resp.size() >= 40) {
            sha256_2 last_block_return;
            std::copy_n(resp.begin(), 32, last_block_return.begin());

            uint64_t block_timestamp = 0;
            std::copy_n(resp.begin() + 32, 8, reinterpret_cast<char*>(&block_timestamp));

            serial_execution.post([this, last_block_return, block_timestamp, mh_addr, index] {
                core_last_block[mh_addr] = block_timestamp;

                if (block_timestamp >= prev_timestamp && !blocks.contains(last_block_return)) {
                    not_actualized[index]++;

                    std::vector<char> last_block;

                    static const sha256_2 zero_block = { { 0 } };
                    if (last_applied_block != zero_block) {
                        last_block.insert(last_block.end(), last_applied_block.begin(), last_applied_block.end());
                    } else {
                        last_block.insert(last_block.end(), proved_block.begin(), proved_block.end());
                    }

                    cores.send_with_callback_to_one(mh_addr, RPC_GET_MISSING_BLOCK_LIST, last_block, [this, mh_addr](const std::vector<char>& resp) {
                        auto block_list = new std::set<sha256_2>();

                        uint index = 0;
                        while (index + 32 <= resp.size()) {
                            sha256_2 block_in_list;

                            std::copy_n(resp.begin() + index, 32, block_in_list.begin());
                            index += 32;

                            block_list->insert(block_in_list);
                        }

                        serial_execution.post([this, block_list, mh_addr] {
                            for (const auto& block_hash : *block_list) {
                                if (!blocks.contains(block_hash)) {
                                    missing_blocks[block_hash].insert(mh_addr);
                                }
                            }
                            delete block_list;
                        });
                    });
                }
            });
        }
    });
}

void ControllerImplementation::actualize_chain()
{
    std::random_device rd;
    std::mt19937 mt(rd());

    auto empty_queue_cores = cores.get_empty_queue_cores();

    for (auto block_it = missing_blocks.begin(); block_it != missing_blocks.end(); block_it = missing_blocks.erase(block_it)) {
        auto& block_hash = block_it->first;
        auto& cores_with_it = block_it->second;

        std::vector<std::string> ready_cores;
        set_intersection(cores_with_it.begin(), cores_with_it.end(), empty_queue_cores.begin(), empty_queue_cores.end(), std::back_inserter(ready_cores));

        if (!blocks.contains(block_hash)) {
            std::vector<char> get_block;
            get_block.insert(get_block.end(), block_hash.begin(), block_hash.end());

            if (ready_cores.empty()) {
                cores.send_with_callback(RPC_GET_BLOCK, get_block, [this](const std::string&, const std::vector<char>& resp) {
                    if (resp.size()) {
                        std::string_view block_sw(resp.data(), resp.size());
                        parse_RPC_PRETEND_BLOCK(block_sw);
                    }
                });
            } else {
                std::uniform_int_distribution<int> dist(0, ready_cores.size() - 1);
                auto rand_int = dist(mt);

                cores.send_with_callback_to_one(ready_cores[rand_int], RPC_GET_BLOCK, get_block, [this](const std::vector<char>& resp) {
                    if (resp.size()) {
                        std::string_view block_sw(resp.data(), resp.size());
                        parse_RPC_PRETEND_BLOCK(block_sw);
                    }
                });
            }

            get_approve_for_block(get_block);
        }
    }
}

void ControllerImplementation::get_approve_for_block(sha256_2& block_hash)
{
    std::vector<char> get_block;
    get_block.insert(get_block.end(), block_hash.begin(), block_hash.end());
    get_approve_for_block(get_block);
}

void ControllerImplementation::get_approve_for_block(std::vector<char>& get_block)
{
    cores.send_no_return(RPC_GET_APPROVE, get_block);
}

}