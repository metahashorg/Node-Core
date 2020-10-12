#include "controller.hpp"
#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_core {

void ControllerImplementation::main_loop()
{
    uint64_t timestamp = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count());
    bool no_sleep = false;

    log_network_statistics(timestamp);

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