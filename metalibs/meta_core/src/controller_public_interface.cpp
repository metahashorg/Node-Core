#include <meta_log.hpp>
#include <meta_constants.hpp>
#include "controller.hpp"

namespace metahash::meta_core {

ControllerImplementation::ControllerImplementation(
    boost::asio::io_context& io_context,
    const std::string& priv_key_line,
    const std::string& path,
    const std::string& proved_hash,
    const std::map<std::string, std::pair<std::string, int>>& core_list,
    const std::pair<std::string, int>& host_port)
    : BC(new meta_chain::BlockChain(io_context))
    , io_context(io_context)
    , serial_execution(io_context)
    , main_loop_timer(io_context, boost::posix_time::milliseconds(10))
    , min_approve(std::ceil(METAHASH_PRIMARY_CORES_COUNT * 51.0 / 100.0))
    , path(path)
    , signer(crypto::hex2bin(priv_key_line))
    , cores(io_context, host_port.first, host_port.second, signer)
{
    DEBUG_COUT("min_approve\t" + std::to_string(min_approve));

    {
        current_cores.insert(current_cores.end(), FOUNDER_WALLETS.begin(), FOUNDER_WALLETS.end());
    }

    {
        std::vector<unsigned char> bin_proved_hash = crypto::hex2bin(proved_hash);
        std::copy_n(bin_proved_hash.begin(), 32, proved_block.begin());
    }

    read_and_apply_local_chain();

    serial_execution.post([this] {
        main_loop();
    });

    listener = new network::meta_server(
        io_context, host_port.first, host_port.second, signer, [this](network::Request& request) -> std::vector<char> {
            return add_pack_to_queue(request);
        });

    cores.init(core_list);
}

std::atomic<std::map<std::string, std::pair<uint, uint>>*>& ControllerImplementation::get_wallet_statistics()
{
    return BC->get_wallet_statistics();
}

std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& ControllerImplementation::get_wallet_request_addresses()
{
    return BC->get_wallet_request_addresses();
}

}