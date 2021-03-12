#include "controller.hpp"
#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_core {

void ControllerImplementation::main_loop()
{
    uint64_t timestamp = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count());
    bool no_sleep = false;

    log_network_statistics(timestamp);

    process_queues();

    if (timestamp - last_sync_timestamp > 60) {
        io_context.post(std::bind(&connection::MetaConnection::sync_core_lists, &cores));
        last_sync_timestamp = timestamp;
    }

    if (timestamp > last_actualization_check_timestamp || prev_timestamp > last_actualization_check_timestamp) {
        last_actualization_check_timestamp = timestamp;
        check_if_chain_actual();
    }

    if (timestamp - last_actualization_timestamp > 5) {
        last_actualization_timestamp = timestamp;
        serial_execution.post(std::bind(&ControllerImplementation::actualize_chain, this));
    }

    no_sleep = check_awaited_blocks();

    if (check_if_can_make_block(timestamp)) {
        no_sleep = try_make_block(timestamp);
    }

    if (no_sleep) {
        serial_execution.post(std::bind(&ControllerImplementation::main_loop, this));
    } else {
        main_loop_timer = boost::asio::deadline_timer(serial_execution, boost::posix_time::milliseconds(10));
        main_loop_timer.async_wait([this](const boost::system::error_code&) {
            main_loop();
        });
    }
}

void ControllerImplementation::process_queues()
{
    {
        const uint64_t LIST_SIZE = 8000;
        {
            static std::vector<transaction::TX*> tx_list(LIST_SIZE, nullptr);
            if (auto size = tx_queue.try_dequeue_bulk(tx_list.begin(), LIST_SIZE)) {
                for (uint i = 0; i < size; i++) {
                    if (tx_list[i]) {
                        transactions.push_back(tx_list[i]);
                    }
                }
                //transactions.insert(transactions.end(), tx_list.begin(), tx_list.begin() + size);
            }
        }
        {
            static std::vector<block::Block*> block_list(LIST_SIZE, nullptr);
            if (auto size = block_queue.try_dequeue_bulk(block_list.begin(), LIST_SIZE)) {
                for (uint i = 0; i < size; i++) {
                    auto block = block_list[i];

                    if (dynamic_cast<block::CommonBlock*>(block)) {
                        missing_blocks.erase(block->get_block_hash());
                        blocks.insert(block);
                    } else if (dynamic_cast<block::RejectedTXBlock*>(block)) {
                        write_block(block);
                    } else {
                        delete block;
                    }
                }
            }
        }
        {
            static std::vector<transaction::ApproveRecord*> approve_list(LIST_SIZE, nullptr);
            if (auto size = approve_queue.try_dequeue_bulk(approve_list.begin(), LIST_SIZE)) {
                for (uint i = 0; i < size; i++) {
                    apply_approve(approve_list[i]);
                }
            }
        }
        {
            static std::vector<std::pair<std::string, sha256_2>> approve_request_list(LIST_SIZE);
            if (auto size = approve_request_queue.try_dequeue_bulk(approve_request_list.begin(), LIST_SIZE)) {
                for (uint i = 0; i < size; i++) {
                    auto&& [core_name, block_hash] = approve_request_list[i];

                    auto approve_list_it = block_approve.find(block_hash);
                    if (approve_list_it == block_approve.end()) {
                        sha256_2 got_block = last_applied_block;
                        while (got_block != block_hash && blocks.contains(got_block)) {
                            got_block = blocks[got_block]->get_prev_hash();
                        }
                        if (got_block == block_hash) {
                            auto* p_ar = new transaction::ApproveRecord;
                            p_ar->make(got_block, signer);
                            p_ar->approve = true;
                            if (!block_approve[got_block].insert({ signer.get_mh_addr(), p_ar }).second) {
                                delete p_ar;
                            }
                            approve_list_it = block_approve.find(block_hash);
                            if (approve_list_it == block_approve.end()) {
                                continue;
                            }
                        } else {
                            continue;
                        }
                    }
                    std::vector<char> approve_data_list;
                    for (auto&& [core_addr, record] : approve_list_it->second) {
                        uint64_t record_size = record->data.size();
                        approve_data_list.insert(approve_data_list.end(), reinterpret_cast<char*>(&record_size), reinterpret_cast<char*>(&record_size) + sizeof(uint64_t));
                        approve_data_list.insert(approve_data_list.end(), record->data.begin(), record->data.end());
                    }

                    if (!approve_data_list.empty()) {
                        cores.send_no_return_to_core(core_name, RPC_APPROVE_LIST, approve_data_list);
                    }
                }
            }
        }
    }
}

bool ControllerImplementation::check_if_can_make_block(const uint64_t& timestamp)
{
    if (not_actualized[0] || not_actualized[1]) {
        return false;
    }
    if (!check_online_nodes(timestamp)) {
        return false;
    }
    if (!master()) {
        return false;
    }
    return true;
}

bool ControllerImplementation::check_awaited_blocks()
{
    static const sha256_2 zero_block = { { 0 } };

    if (last_applied_block != zero_block) {
        if (blocks.contains_next(last_applied_block)) {
            auto* block = blocks.get_next(last_applied_block);
            if (check_block_for_appliance_and_break_on_corrupt_block(block)) {
                return true;
            }
        }
    } else {
        if (proved_block != zero_block) {
            if (blocks.contains(proved_block)) {
                auto* curr_block = blocks[proved_block];
                auto prev_hash = curr_block->get_prev_hash();

                while (curr_block->get_block_type() != BLOCK_TYPE_STATE && prev_hash != zero_block) {
                    if (blocks.contains(prev_hash)) {
                        curr_block = blocks[prev_hash];
                        prev_hash = curr_block->get_prev_hash();
                    } else {
                        missing_blocks[prev_hash];
                        serial_execution.post(std::bind(&ControllerImplementation::actualize_chain, this));
                        return false;
                    }
                }

                if (check_block_for_appliance_and_break_on_corrupt_block(curr_block)) {
                    return true;
                }
            }
        } else if (blocks.contains_next(proved_block)) {
            auto* block = blocks.get_next(last_applied_block);
            if (check_block_for_appliance_and_break_on_corrupt_block(block)) {
                return true;
            }
        }
    }

    return false;
}

}