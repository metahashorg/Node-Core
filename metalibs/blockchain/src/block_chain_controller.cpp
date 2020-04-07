#include <utility>

#include "controller.hpp"
#include <blockchain.h>
#include <meta_log.hpp>

BlockChainController::BlockChainController(
    ThreadPool& TP,
    const std::string& priv_key_line,
    const std::string& path,
    const std::string& proved_hash,
    const std::set<std::pair<std::string, int>>& core_list,
    const std::pair<std::string, int>& host_port,
    bool test)
    : CI(new ControllerImplementation(TP, priv_key_line, path, proved_hash, core_list, host_port, test))
{
}

std::string BlockChainController::add_pack_to_queue(std::string_view pack, std::string_view url, std::string_view sign, std::string_view pubk)
{
    return CI->add_pack_to_queue(pack, url, sign, pubk);
}

std::string BlockChainController::get_str_address()
{
    return CI->get_str_address();
}

std::string BlockChainController::get_last_block_str()
{
    return CI->get_last_block_str();
}

std::atomic<std::map<std::string, std::pair<uint, uint>>*>& BlockChainController::get_wallet_statistics()
{
    return CI->get_wallet_statistics();
}

std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& BlockChainController::get_wallet_request_addreses()
{
    return CI->get_wallet_request_addreses();
}

BlockChainController::~BlockChainController()
{
    delete CI;
}
