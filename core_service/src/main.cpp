#include "core_service.h"

#include <blockchain.h>
#include <meta_log.hpp>
#include <version.h>

int main(int argc, char** argv)
{
    std::string network;
    std::string host;
    int tx_port = 8181;
    std::string path;
    std::string known_hash;
    std::string key;
    std::map<std::string, std::pair<std::string, int>> core_list;

    static const std::string version = std::string(VERSION_MAJOR) + "." + std::string(VERSION_MINOR) + "." + std::string(GIT_COUNT) + "." + std::string(GIT_COMMIT_HASH);
    DEBUG_COUT(version);

    if (argc < 2) {
        DEBUG_COUT("Need Configuration File As Parameter");
        print_config_file_params_and_exit();
    }

    parse_settings(std::string(argv[1]), network, host, tx_port, path, known_hash, key, core_list);

    auto thread_count = std::thread::hardware_concurrency() - 1;
    boost::asio::io_context io_context(thread_count);
    auto&& [threads, work] = metahash::pool::thread_pool(io_context, thread_count);

    BlockChainController blockChainController(io_context, key, path, known_hash, core_list, { host, tx_port });

    libevent(io_context, blockChainController.get_wallet_statistics(), blockChainController.get_wallet_request_addresses(), "wsstata.metahash.io", 80, "net-test");

    io_context.run();

    for (auto& t : threads) {
        t.join();
    }

    return EXIT_SUCCESS;
}