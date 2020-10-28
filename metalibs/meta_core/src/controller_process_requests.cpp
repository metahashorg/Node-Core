#include "controller.hpp"

#include <meta_constants.hpp>
#include <meta_log.hpp>

#include <list>

namespace metahash::meta_core {

void ControllerImplementation::parse_RPC_TX(std::string_view pack)
{
    auto* tx_list = new std::list<transaction::TX*>();

    uint64_t index = 0;
    uint64_t tx_size;
    std::string_view tx_size_arr(&pack[index], pack.size() - index);
    uint64_t varint_size = crypto::read_varint(tx_size, tx_size_arr);
    if (varint_size < 1) {
        DEBUG_COUT("corrupt varint size");
        return;
    }
    index += varint_size;

    while (tx_size > 0) {
        if (index + tx_size >= pack.size()) {
            DEBUG_COUT("corrupt tx size");
            return;
        }
        std::string_view tx_sw(&pack[index], tx_size);
        index += tx_size;

        auto p_tx = new transaction::TX;
        if (p_tx->parse(tx_sw)) {
            tx_list->push_back(p_tx);
        } else {
            delete p_tx;
            DEBUG_COUT("corrupt tx");
        }

        tx_size_arr = std::string_view(&pack[index], pack.size() - index);
        varint_size = crypto::read_varint(tx_size, tx_size_arr);
        if (varint_size < 1) {
            DEBUG_COUT("corrupt varint size");
            return;
        }
        index += varint_size;
    }

    if (tx_list->empty()) {
        delete tx_list;
    } else {
        serial_execution.post([this, tx_list] {
            transactions.insert(transactions.end(), tx_list->begin(), tx_list->end());
            delete tx_list;
        });
    }
}

void ControllerImplementation::parse_RPC_PRETEND_BLOCK(std::string_view pack)
{
    std::string_view block_sw(pack);
    auto* block = block::parse_block(block_sw);

    if (block) {
        if (dynamic_cast<block::CommonBlock*>(block)) {
            serial_execution.post([this, block] {
                missing_blocks.erase(block->get_block_hash());

                blocks.insert(block);
            });
        } else if (dynamic_cast<block::RejectedTXBlock*>(block)) {
            serial_execution.post([this, block] {
                write_block(block);
            });
        } else {
            delete block;
        }
    }
}

void ControllerImplementation::parse_RPC_APPROVE(std::string_view pack)
{
    std::string_view approve_sw(pack);
    auto* p_ar = new transaction::ApproveRecord;
    if (p_ar->parse(approve_sw)) {
        p_ar->approve = true;

        serial_execution.post([this, p_ar] {
            apply_approve(p_ar);
        });
    } else {
        delete p_ar;
    }
}

void ControllerImplementation::parse_RPC_DISAPPROVE(std::string_view pack)
{
    std::string_view approve_sw(pack);
    auto* p_ar = new transaction::ApproveRecord;
    if (p_ar->parse(approve_sw)) {
        p_ar->approve = false;

        serial_execution.post([this, p_ar] {
            apply_approve(p_ar);
        });
    } else {
        delete p_ar;
    }
}

std::vector<char> ControllerImplementation::parse_RPC_GET_APPROVE(std::string_view pack)
{
    if (pack.size() < 32) {
        DEBUG_COUT("pack.size() < 32");
        return std::vector<char>();
    }

    sha256_2 approve_wanted_block;
    std::copy_n(pack.begin(), 32, approve_wanted_block.begin());

    std::shared_lock a_lock(block_approve_lock);
    auto approve_list_it = block_approve.find(approve_wanted_block);
    a_lock.unlock();

    if (approve_list_it == block_approve.end()) {
        sha256_2 got_block = last_applied_block;

        while (got_block != approve_wanted_block && blocks.contains(got_block)) {
            got_block = blocks[got_block]->get_prev_hash();
        }

        if (got_block == approve_wanted_block) {
            auto* p_ar = new transaction::ApproveRecord;
            p_ar->make(got_block, signer);
            p_ar->approve = true;
            std::unique_lock ulock(block_approve_lock);
            if (!block_approve[got_block].insert({ signer.get_mh_addr(), p_ar }).second) {
                //DEBUG_COUT("APPROVE ALREADY PRESENT NOT CREATED");
                delete p_ar;
            }
            approve_list_it = block_approve.find(approve_wanted_block);
            if (approve_list_it == block_approve.end()) {
                return std::vector<char>();
            }
        } else {
            return std::vector<char>();
        }
    }

    std::vector<char> approve_data_list;
    for (auto&& [core_addr, record] : approve_list_it->second) {
        uint64_t record_size = record->data.size();
        approve_data_list.insert(approve_data_list.end(), reinterpret_cast<char*>(&record_size), reinterpret_cast<char*>(&record_size) + sizeof(uint64_t));
        approve_data_list.insert(approve_data_list.end(), record->data.begin(), record->data.end());
    }
    return approve_data_list;
}

std::vector<char> ControllerImplementation::parse_RPC_LAST_BLOCK(std::string_view)
{
    std::vector<char> last_block;
    sha256_2 got_block;
    uint64_t got_timestamp;

    if (master()) {

        if (blocks.contains(last_created_block)) {
            got_block = last_created_block;
            got_timestamp = blocks[last_created_block]->get_block_timestamp();
        } else {
            got_block = last_applied_block;
            got_timestamp = prev_timestamp;
        }
    } else {
        got_block = last_applied_block;
        got_timestamp = prev_timestamp;
    }

    last_block.insert(last_block.end(), got_block.begin(), got_block.end());
    char* p_timestamp = reinterpret_cast<char*>(&got_timestamp);
    last_block.insert(last_block.end(), p_timestamp, p_timestamp + 8);

    return last_block;
}

std::vector<char> ControllerImplementation::parse_RPC_GET_BLOCK(std::string_view pack)
{
    if (pack.size() < 32) {
        return std::vector<char>();
    }

    sha256_2 block_hash;
    std::copy_n(pack.begin(), 32, block_hash.begin());

    if (blocks.contains(block_hash)) {
        return blocks[block_hash]->get_data();
    }

    return std::vector<char>();
}

std::vector<char> ControllerImplementation::parse_RPC_GET_CHAIN(std::string_view pack)
{
    if (pack.size() < 32) {
        return std::vector<char>();
    }

    sha256_2 prev_block;
    std::copy_n(pack.begin(), 32, prev_block.begin());

    std::vector<char> chain;
    sha256_2 got_block = last_applied_block;

    while (got_block != prev_block && blocks.contains(got_block)) {
        auto& block_data = blocks[got_block]->get_data();

        uint64_t block_size = block_data.size();
        chain.insert(chain.end(), reinterpret_cast<char*>(&block_size), reinterpret_cast<char*>(&block_size) + sizeof(uint64_t));
        chain.insert(chain.end(), block_data.begin(), block_data.end());

        got_block = blocks[got_block]->get_prev_hash();
    }

    if (master()) {
        auto& block_data = blocks[last_created_block]->get_data();
        uint64_t block_size = block_data.size();
        chain.insert(chain.end(), reinterpret_cast<char*>(&block_size), reinterpret_cast<char*>(&block_size) + sizeof(uint64_t));
        chain.insert(chain.end(), block_data.begin(), block_data.end());
    }

    return chain;
}

std::vector<char> ControllerImplementation::parse_RPC_GET_MISSING_BLOCK_LIST(std::string_view pack)
{
    std::vector<char> chain;

    if (pack.size() < 32) {
        return chain;
    }

    sha256_2 prev_block;
    std::copy_n(pack.begin(), 32, prev_block.begin());

    sha256_2 got_block = last_applied_block;

    while (got_block != prev_block && blocks.contains(got_block)) {
        chain.insert(chain.end(), got_block.begin(), got_block.end());

        got_block = blocks[got_block]->get_prev_hash();
    }

    if (blocks.contains(prev_block)) {
        chain.insert(chain.end(), prev_block.begin(), prev_block.end());
    }

    if (master()) {
        chain.insert(chain.end(), last_created_block.begin(), last_created_block.end());
    }

    return chain;
}

std::vector<char> ControllerImplementation::parse_RPC_GET_CORE_LIST(std::string_view pack)
{
    cores.add_cores(pack);

    return cores.get_core_list();
}

void ControllerImplementation::parse_RPC_CORE_LIST_APPROVE(std::string core, std::string_view pack)
{
    uint64_t current_timestamp = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count());
    uint64_t current_generation = current_timestamp / CORE_LIST_RENEW_PERIOD;
    std::string list(pack);

    serial_execution.post([this, core, list, current_generation] {
        proposed_cores[current_generation][list].insert(core);
    });
}

}