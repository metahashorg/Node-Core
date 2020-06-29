#include <arpa/inet.h>
#include <deque>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

#include <blockchain.h>
#include <meta_log.hpp>
#include <mhcurl.hpp>
#include <open_ssl_decor.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <statics.hpp>
#include <version.h>

[[noreturn]] void print_config_file_params_and_exit()
{
    static const std::string version = std::string(VESION_MAJOR) + "." + std::string(VESION_MINOR) + "." + std::string(GIT_COUNT) + "." + std::string(GIT_COMMIT_HASH);
    DEBUG_COUT("");
    DEBUG_COUT(version);
    DEBUG_COUT("Configureation file parameters:");
    DEBUG_COUT("Line 1: network name [net-main|net-dev|net-test]");
    DEBUG_COUT("Line 2: listening host port(127.0.0.1 9999)");
    DEBUG_COUT("Line 3: metachain path");
    DEBUG_COUT("Line 3: trusted blosk hash (0x0000000000000000000000000000000000000000000000000000000000000000 for full blockchain)");
    DEBUG_COUT("Line 4: private key");
    DEBUG_COUT("Line 5...N: core list");

    DEBUG_COUT("Caught SIGTERM");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    exit(1);
}

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
    std::atomic<std::map<std::string, std::pair<uint, uint>>*>& statistics,
    std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& request_addreses,
    const std::string& host, int port,
    const std::string& network);

int main(int argc, char** argv)
{
    std::string network;
    std::string host;
    int tx_port = 8181;
    std::string path;
    std::string known_hash;
    std::string key;
    std::map<std::string, std::pair<std::string, int>> core_list;

    bool skip_last_forging_and_state = false;

    if (argc < 2) {
        DEBUG_COUT("Need Configuration File As Parameter");
        print_config_file_params_and_exit();
    }

    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "test") {
            DEBUG_COUT("TEST MODE");
            skip_last_forging_and_state = true;
        }
    }

    parse_settings(std::string(argv[1]), network, host, tx_port, path, known_hash, key, core_list);

    int thread_count = std::thread::hardware_concurrency();
    boost::asio::io_context io_context(thread_count);
    auto&& [threads, work] = thread_pool(io_context, thread_count);

    BlockChainController blockChainController(io_context, key, path, known_hash, core_list, { host, tx_port }, skip_last_forging_and_state);

    std::thread(libevent, std::ref(blockChainController.get_wallet_statistics()), std::ref(blockChainController.get_wallet_request_addresses()), "wsstata.metahash.io", 80, "net-test").detach();

    io_context.run();

    for (auto& t : threads) {
        t.join();
    }

    return EXIT_SUCCESS;
}

__attribute__((__noreturn__)) void libevent(
    std::atomic<std::map<std::string, std::pair<uint, uint>>*>& statistics,
    std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& request_addreses,
    const std::string& host, int port,
    const std::string& network)
{
    std::string key_type = network == "net-dev" ? "keys_tmh" : "keys_mth";

    CurlFetch CF(host, port);
    for (;;) {
        std::deque<std::pair<std::string, uint64_t>>* p_addreses;
        while (true) {
            std::deque<std::pair<std::string, uint64_t>>* null_adr = nullptr;
            p_addreses = request_addreses.load();
            if (request_addreses.compare_exchange_strong(p_addreses, null_adr)) {
                break;
            }
        }

        if (p_addreses != nullptr) {
            std::deque<std::pair<std::string, uint64_t>> addreses(*p_addreses);

            {
                std::deque<std::pair<std::string, uint64_t>>* null_adr = nullptr;
                if (!request_addreses.compare_exchange_strong(null_adr, p_addreses)) {
                    delete p_addreses;
                }
            }

            bool first = true;
            std::string request_string;
            request_string += R"({"app":"CoreServiceIP","data":{")" + key_type + "\":[";
            for (const auto& addr : addreses) {
                if (first) {
                    first = false;
                } else {
                    request_string += ",";
                }
                request_string += "\"" + addr.first + "\"";
            }
            request_string += "]}}";

            DEBUG_COUT(std::to_string(request_string.size()));

            static const std::string path = "/";
            std::string response;
            for (;;) {
                if (CF.post("/", request_string, response)) {
                    break;
                }
            }

            DEBUG_COUT(std::to_string(response.size()));

            rapidjson::Document addr_stat;
            if (!addr_stat.Parse(response.c_str()).HasParseError()) {
                if (addr_stat.HasMember("data") && addr_stat["data"].IsObject()) {
                    const auto& data = addr_stat["data"];

                    if (data.HasMember(key_type.c_str()) && data[key_type.c_str()].IsArray()) {
                        const auto& addrs = data[key_type.c_str()];

                        std::map<std::string, std::map<std::string, std::tuple<uint64_t, uint64_t, uint64_t>>> addr_stat_map;

                        for (uint i = 0; i < addrs.Size(); i++) {
                            auto& record = addrs[i];
                            if (record.HasMember("hwid") && record["hwid"].IsString() && record.HasMember("address") && record["address"].IsString()) {

                                std::string address(record["address"].GetString());
                                std::string hwid(record["hwid"].GetString());
                                uint64_t ip = (record.HasMember("ip") && record["ip"].IsUint64()) ? record["ip"].GetUint64() : 0;
                                uint64_t online = ((record.HasMember("online_day") && record["online_day"].IsUint64()) ? record["online_day"].GetUint64() : 0) / (60 * 4 - 1);
                                uint64_t tickets = (record.HasMember("tickets") && record["tickets"].IsUint64()) ? record["tickets"].GetUint64() : 0;

                                addr_stat_map[address].insert({ hwid, { ip, online, tickets } });
                            }
                        }

                        std::set<uint64_t> ips;
                        std::set<std::string> hwids_ticket;
                        std::set<std::string> hwids_online;
                        auto* p_got_statistics = new std::map<std::string, std::pair<uint, uint>>();
                        auto& got_statistics = *p_got_statistics;

                        for (const auto& addr_pair : addreses) {
                            for (const auto& hwid_stat : addr_stat_map[addr_pair.first]) {
                                bool got_one = false;

                                uint64_t ip = 0;
                                uint64_t online = 0;
                                uint64_t tickets = 0;

                                std::tie(ip, online, tickets) = hwid_stat.second;

                                if (online && hwids_online.find(hwid_stat.first) == hwids_online.end()) {
                                    if (ip) {
                                        if (ips.insert(ip).second) {
                                            hwids_online.insert(hwid_stat.first);
                                            got_statistics[addr_pair.first].first += online;
                                            got_one = true;
                                        }
                                    } else {
                                        hwids_online.insert(hwid_stat.first);
                                        got_statistics[addr_pair.first].first += online;
                                        got_one = true;
                                    }
                                }

                                if (addr_pair.second >= 1000 MHC && tickets && hwids_ticket.insert(hwid_stat.first).second) {
                                    got_statistics[addr_pair.first].second += tickets;
                                    got_one = true;
                                }

                                if (got_one) {
                                    break;
                                }
                            }
                        }

                        DEBUG_COUT("got info for \t" + std::to_string(got_statistics.size()) + "\taddresses");

                        for (;;) {
                            std::map<std::string, std::pair<uint, uint>>* got_statistics_prev = statistics.load();
                            if (statistics.compare_exchange_strong(got_statistics_prev, p_got_statistics)) {

                                delete got_statistics_prev;

                                break;
                            }
                        }
                    }
                }
            } else {
                DEBUG_COUT("error in service response");
                DEBUG_COUT(response);
            }
        } else {
            DEBUG_COUT("no adr list");
        }

        std::this_thread::sleep_for(std::chrono::minutes(15));
    }
    exit(0);
}

void parse_settings(
    const std::string& file_name,
    std::string& network,
    std::string& tx_host,
    int& tx_port,
    std::string& path,
    std::string& hash,
    std::string& key,
    std::map<std::string, std::pair<std::string, int>>& core_list)
{
    std::ifstream ifs(file_name);
    std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));

    rapidjson::Document config_json;
    if (!config_json.Parse(content.c_str()).HasParseError()) {

        if (config_json.HasMember("network") && config_json["network"].IsString()) {
            network = config_json["network"].GetString();
            if (network != "net-main" && network != "net-dev" && network != "net-test") {
                DEBUG_COUT("Unknown network name:\t" + network);
                print_config_file_params_and_exit();
            }

            DEBUG_COUT("Network name:\t" + network);
        } else {
            DEBUG_COUT("Missing network name");
            print_config_file_params_and_exit();
        }

        if (config_json.HasMember("hostname") && config_json["hostname"].IsString() && config_json.HasMember("port") && config_json["port"].IsUint() && config_json["port"].GetInt() != 0) {
            tx_host = config_json["hostname"].GetString();
            tx_port = config_json["port"].GetInt();
        } else {
            DEBUG_COUT("Invalid or missing listening host or port");
            print_config_file_params_and_exit();
        }

        if (config_json.HasMember("path") && config_json["path"].IsString()) {
            path = config_json["path"].GetString();

            if (std::filesystem::path dir(path); !std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
                DEBUG_COUT("Invalid metachain path:\t" + path);
                print_config_file_params_and_exit();
            }

            DEBUG_COUT("Metachain path:\t" + path);
        } else {
            DEBUG_COUT("Missing metachain path");
            print_config_file_params_and_exit();
        }
        if (config_json.HasMember("hash") && config_json["hash"].IsString()) {
            hash = config_json["hash"].GetString();
            auto hash_as_vec = metahash::crypto::hex2bin(hash);
            if (hash_as_vec.size() != 32) {
                DEBUG_COUT("Invalid trusted block hash");
                print_config_file_params_and_exit();
            }

            DEBUG_COUT("Trusted block hash:\t" + hash);
        } else {
            DEBUG_COUT("Missing trusted hash");
            print_config_file_params_and_exit();
        }
        if (config_json.HasMember("key") && config_json["key"].IsString()) {
            key = config_json["key"].GetString();

            std::vector<unsigned char> priv_k = metahash::crypto::hex2bin(key);
            std::vector<char> PubKey;
            if (!metahash::crypto::generate_public_key(PubKey, priv_k)) {
                DEBUG_COUT("Error while parsing Private key");
                print_config_file_params_and_exit();
            }

            std::array<char, 25> addres = metahash::crypto::get_address(PubKey);
            std::string Text_addres = "0x" + metahash::crypto::bin2hex(addres);
            DEBUG_COUT("got key for address:\t" + Text_addres);

        } else {
            DEBUG_COUT("Missing Private key");
            print_config_file_params_and_exit();
        }
        if (config_json.HasMember("cores") && config_json["cores"].IsArray()) {
            auto& v_list = config_json["cores"];

            for (uint i = 0; i < v_list.Size(); i++) {
                auto& record = v_list[i];

                if (record.HasMember("address") && record["address"].IsString()
                    && record.HasMember("host") && record["host"].IsString()
                    && record.HasMember("port") && record["port"].IsUint()
                    && record["port"].GetUint() != 0) {

                    DEBUG_COUT("Core\t" + std::string(record["address"].GetString()) + "\t" + std::string(record["host"].GetString()) + "\t" + std::to_string(record["port"].GetUint()));
                    core_list.insert({ record["address"].GetString(), { record["host"].GetString(), record["port"].GetUint() } });
                }
            }

            if (core_list.empty()) {
                DEBUG_COUT("WARNING: No cores present in configuration file");
            }
        } else {
            DEBUG_COUT("WARNING: No cores present in configuration file");
        }
    } else {
        DEBUG_COUT("Invalid Configuration File");
        print_config_file_params_and_exit();
    }
}