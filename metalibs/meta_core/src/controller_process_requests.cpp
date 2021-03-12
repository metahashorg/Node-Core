#include "controller.hpp"

#include <meta_constants.hpp>
#include <meta_log.hpp>

#include <list>

namespace metahash::meta_core {

void ControllerImplementation::parse_RPC_TX(std::string_view pack)
{
    std::vector<transaction::TX*> tx_list(pack.size()/128);

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
            tx_list.push_back(p_tx);
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

    if (tx_list.size()) {
        tx_queue.enqueue_bulk(tx_list.begin(), tx_list.size());
    }
}

void ControllerImplementation::parse_RPC_PRETEND_BLOCK(std::string_view pack)
{
    std::string_view block_sw(pack);
    auto* block = block::parse_block(block_sw);

    if (block) {
        if (dynamic_cast<block::CommonBlock*>(block) || dynamic_cast<block::RejectedTXBlock*>(block)) {
            block_queue.enqueue(block);
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

        approve_queue.enqueue(p_ar);
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

        approve_queue.enqueue(p_ar);
    } else {
        delete p_ar;
    }
}

void ControllerImplementation::parse_RPC_APPROVE_LIST(std::string_view pack)
{
    uint index = 0;
    while (index + 8 <= pack.size()) {
        uint64_t approve_size = *(reinterpret_cast<const uint64_t*>(&pack[index]));
        index += 8;

        if (index + approve_size > pack.size()) {
            break;
        }
        std::string_view approve_sw(&pack[index], approve_size);
        parse_RPC_APPROVE(approve_sw);
        index += approve_size;
    }
}

void ControllerImplementation::parse_RPC_GET_APPROVE(const std::string& core, std::string_view pack)
{
    if (pack.size() < 32) {
        DEBUG_COUT("pack.size() < 32");
        return;
    }

    sha256_2 approve_wanted_block;
    std::copy_n(pack.begin(), 32, approve_wanted_block.begin());

    approve_request_queue.enqueue({core, approve_wanted_block});
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

void ControllerImplementation::parse_RPC_CORE_LIST_APPROVE(const std::string& core, std::string_view pack)
{
    uint64_t current_timestamp = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count());
    uint64_t current_generation = current_timestamp / CORE_LIST_RENEW_PERIOD;
    std::string list(pack);

    serial_execution.post([this, core, list, current_generation] {
        proposed_cores[current_generation][list].insert(core);
    });
}

}