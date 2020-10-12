#include <meta_constants.hpp>
#include <meta_log.hpp>

#include "controller.hpp"

namespace metahash::meta_core {

void ControllerImplementation::check_if_chain_actual()
{
    DEBUG_COUT("check_if_chain_actual");

    std::map<std::string, std::vector<char>> last_block_on_cores = cores.send_with_return(RPC_LAST_BLOCK, std::vector<char>());

    std::set<std::string> cores_with_missing_block;
    std::set<sha256_2> missing_blocks;

    serial_execution.post([this, last_block_on_cores] {
        bool need_actualization = false;
        for (auto&& [core_addr, block_info] : last_block_on_cores) {
            if (block_info.size() >= 40) {
                sha256_2 last_block_return;
                std::copy_n(block_info.begin(), 32, last_block_return.begin());

                uint64_t block_timestamp = 0;
                std::copy_n(block_info.begin() + 32, 8, reinterpret_cast<char*>(&block_timestamp));

                if (block_timestamp > prev_timestamp && !blocks.count(last_block_return)) {
                    DEBUG_COUT("core\t" + core_addr + "have more recent block\t" + crypto::bin2hex(last_block_return));
                    need_actualization = true;
                }
            }
        }
        if (need_actualization) {
            DEBUG_COUT("need_actualization");
            actualize_chain();
        }
    });
}

void ControllerImplementation::actualize_chain()
{
    DEBUG_COUT("actualize_chain");

    std::map<std::string, std::vector<char>> last_block_on_cores = cores.send_with_return(RPC_LAST_BLOCK, std::vector<char>());

    std::set<std::string> cores_with_missing_block;
    std::set<sha256_2> missing_blocks;
    for (auto&& [core_addr, block_info] : last_block_on_cores) {
        if (block_info.size() >= 40) {
            sha256_2 last_block_return;
            std::copy_n(block_info.begin(), 32, last_block_return.begin());

            uint64_t block_timestamp = 0;
            std::copy_n(block_info.begin() + 32, 8, reinterpret_cast<char*>(&block_timestamp));

            if (block_timestamp > prev_timestamp && !blocks.count(last_block_return)) {
                DEBUG_COUT("core\t" + core_addr + "have more recent block\t" + crypto::bin2hex(last_block_return));
                cores_with_missing_block.insert(core_addr);
                missing_blocks.insert(last_block_return);
            }
        }
    }

    if (!cores_with_missing_block.empty()) {
        DEBUG_COUT("GET BLOCKCHAIN START");

        std::unordered_map<sha256_2, block::Block*, crypto::Hasher> block_tree;
        std::unordered_map<sha256_2, block::Block*, crypto::Hasher> prev_tree;

        uint64_t failed = 0;

        for (;;) {
            for (const auto& core : cores_with_missing_block) {
                if (missing_blocks.empty() || failed > 100) {
                    DEBUG_COUT("GET BLOCKCHAIN COMPLETE");
                    apply_block_chain(block_tree, prev_tree, "metachain network", true);
                    DEBUG_COUT("APPLY BLOCKCHAIN COMPLETE");
                    return;
                }

                sha256_2 requested_block = *missing_blocks.begin();

                std::vector<char> block_as_vector;
                block_as_vector.insert(block_as_vector.end(), requested_block.begin(), requested_block.end());

                DEBUG_COUT(crypto::bin2hex(requested_block) + " <- " + core);
                std::vector<char> return_data = cores.send_with_return_to_core(core, RPC_GET_BLOCK, block_as_vector);

                if (return_data.empty()) {
                    DEBUG_COUT("Block is empty");
                } else {
                    missing_blocks.erase(missing_blocks.begin());
                }

                std::string_view block_as_sw(return_data.data(), return_data.size());
                auto* block = block::parse_block(block_as_sw);
                if (block) {
                    if (dynamic_cast<block::CommonBlock*>(block)) {
                        failed = 0;
                        if (!block_tree.insert({ block->get_block_hash(), block }).second) {
                            DEBUG_COUT("Duplicate block in chain\t" + crypto::bin2hex(block->get_block_hash()));
                            delete block;
                            block = nullptr;
                        } else if (!prev_tree.insert({ block->get_prev_hash(), block }).second) {
                            DEBUG_COUT("Branches in block chain\t" + crypto::bin2hex(block->get_prev_hash()) + "\t->\t" + crypto::bin2hex(block->get_block_hash()));
                            block_tree.erase(block->get_block_hash());
                            delete block;
                            block = nullptr;
                        } else if (!blocks.count(block->get_prev_hash())) {
                            if (block->get_prev_hash() == sha256_2 {}) {
                                DEBUG_COUT("Got complete chain. Previous block is zero block");
                            } else {
                                missing_blocks.insert(block->get_prev_hash());
                            }
                        }
                    } else {
                        failed++;
                        delete block;
                        block = nullptr;
                    }
                } else {
                    failed++;
                    DEBUG_COUT("Parse block error");
                }
            }
        }
    }
}

}