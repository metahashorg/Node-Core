#ifndef METANET_CORE_SERVICE_H
#define METANET_CORE_SERVICE_H

#include <atomic>
#include <deque>
#include <map>
#include <string>

#include <boost/asio.hpp>

void print_config_file_params_and_exit();

void parse_settings(
    const std::string& file_name,
    std::string& network,
    std::string& tx_host,
    int& tx_port,
    std::string& path,
    std::string& hash,
    std::string& key,
    std::map<std::string, std::pair<std::string, int>>& core_list);

void libevent(
    boost::asio::io_context& ioc,
    std::atomic<std::map<std::string, std::pair<uint, uint>>*>& statistics,
    std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& request_addreses,
    const std::string& host, int port,
    const std::string& network);

#endif //METANET_CORE_SERVICE_H
