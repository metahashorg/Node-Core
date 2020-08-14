#include "controller.hpp"
#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_core {

void ControllerImplementation::main_loop()
{
    uint64_t timestamp = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count());
    bool no_sleep = false;

    if (timestamp != dbg_timestamp) {
        DEBUG_COUT("P\tTX\tGCL\tA\tD\tLB\tGB\tGC\tCLA\tPB\tN");
        DEBUG_COUT(std::to_string(dbg_RPC_PING) +
            "\t" + std::to_string(dbg_RPC_TX) +
            "\t" + std::to_string(dbg_RPC_GET_CORE_LIST) +
            "\t" + std::to_string(dbg_RPC_APPROVE) +
            "\t" + std::to_string(dbg_RPC_DISAPPROVE) +
            "\t" + std::to_string(dbg_RPC_LAST_BLOCK) +
            "\t" + std::to_string(dbg_RPC_GET_BLOCK) +
            "\t" + std::to_string(dbg_RPC_GET_CHAIN) +
            "\t" + std::to_string(dbg_RPC_CORE_LIST_APPROVE) +
            "\t" + std::to_string(dbg_RPC_PRETEND_BLOCK) +
            "\t" + std::to_string(dbg_RPC_NONE));

        dbg_RPC_PING = 0;
        dbg_RPC_TX = 0;
        dbg_RPC_GET_CORE_LIST = 0;
        dbg_RPC_APPROVE = 0;
        dbg_RPC_DISAPPROVE = 0;
        dbg_RPC_LAST_BLOCK = 0;
        dbg_RPC_GET_BLOCK = 0;
        dbg_RPC_GET_CHAIN = 0;
        dbg_RPC_CORE_LIST_APPROVE = 0;
        dbg_RPC_PRETEND_BLOCK = 0;
        dbg_RPC_NONE = 0;

        dbg_timestamp = timestamp;
    }

    if (timestamp - last_sync_timestamp > 60) {
        DEBUG_COUT("sync_core_lists");
        io_context.post([this] { cores.sync_core_lists(); });

        last_sync_timestamp = timestamp;
    }

    if (check_online_nodes()) {
        {
            if (prev_timestamp > last_actualization_timestamp) {
                last_actualization_timestamp = prev_timestamp;
            }

            uint64_t sync_interval = master() ? 60 : 1;

            if (timestamp - last_actualization_timestamp > sync_interval) {
                last_actualization_timestamp = timestamp;
                io_context.post([this] {
                    check_if_chain_actual();
                });

                no_sleep = true;
            }
        }

        for (auto&& [hash, block] : blocks) {
            if (!block_approve[hash].count(signer.get_mh_addr()) && !block_disapprove[hash].count(signer.get_mh_addr())) {
                if (block->get_prev_hash() == last_applied_block) {
                    if (BC->can_apply_block(block)) {
                        approve_block(block);
                    } else {
                        disapprove_block(block);
                    }

                    no_sleep = true;
                }
            }
        }

        if (master() && try_make_block()) {
            no_sleep = true;
        }
    } else {
        if (timestamp - last_actualization_timestamp > 60) {
            last_actualization_timestamp = timestamp;
            serial_execution.post([this] {
                actualize_chain();
            });
        }
    }

    if (no_sleep) {
        serial_execution.post([this] {
            main_loop();
        });
    } else {
        main_loop_timer = boost::asio::deadline_timer(io_context, boost::posix_time::milliseconds(1));
        main_loop_timer.async_wait([this](const boost::system::error_code&) {
            serial_execution.post([this] {
                main_loop();
            });
        });
    }
}

}