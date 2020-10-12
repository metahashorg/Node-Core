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

    for (auto&& [hash, block] : await_blocks) {
        static const sha256_2 zero_block = { { 0 } };

        if (last_applied_block != zero_block) {
            if (block->get_prev_hash() == last_applied_block) {
                no_sleep = true;
                if (check_block_for_appliance_and_break_on_corrupt_block(hash, block)) {
                    break;
                }
            }
        } else {
            if (proved_block != zero_block && hash == proved_block) {
                no_sleep = true;
                if (check_block_for_appliance_and_break_on_corrupt_block(hash, block)) {
                    break;
                }
            } else if (proved_block == zero_block && block->get_prev_hash() == proved_block) {
                no_sleep = true;
                if (check_block_for_appliance_and_break_on_corrupt_block(hash, block)) {
                    break;
                }
            }
        }
    }

    if (check_online_nodes(timestamp) && master() && try_make_block(timestamp)) {
        no_sleep = true;
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

}