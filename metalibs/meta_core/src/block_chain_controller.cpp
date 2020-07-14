#include "controller.hpp"
#include <blockchain.h>

BlockChainController::BlockChainController(
    boost::asio::io_context& io_context,
    const std::string& priv_key_line,
    const std::string& path,
    const std::string& proved_hash,
    const std::map<std::string, std::pair<std::string, int>>& core_list,
    const std::pair<std::string, int>& host_port)
    : CI(new metahash::meta_core::ControllerImplementation(io_context, priv_key_line, path, proved_hash, core_list, host_port))
{
}

std::atomic<std::map<std::string, std::pair<uint, uint>>*>& BlockChainController::get_wallet_statistics()
{
    return CI->get_wallet_statistics();
}

std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& BlockChainController::get_wallet_request_addresses()
{
    return CI->get_wallet_request_addresses();
}

BlockChainController::~BlockChainController()
{
    delete CI;
}