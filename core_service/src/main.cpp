#include <csignal>

#include <arpa/inet.h>
#include <climits>
#include <csignal>
#include <deque>
#include <filesystem>
#include <fstream>
#include <list>
#include <netdb.h>
#include <netinet/in.h>
#include <set>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

#include "blockserver.h"
#include "version.h"

#include <blockchain.h>
// #include <http_io_data.hpp>
#include <meta_log.hpp>
#include <open_ssl_decor.h>
#include <statics.hpp>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <mhcurl.hpp>

std::string getMyIp()
{
    std::string MyIP;
    const char* statistics_server = "172.104.236.166";
    int statistics_port = 5797;

    struct sockaddr_in serv {
    };

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    //Socket could not be created
    if (sock < 0) {
        perror("Socket error");
    }

    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr(statistics_server);
    serv.sin_port = htons(statistics_port);

    connect(sock, (const struct sockaddr*)&serv, sizeof(serv));

    struct sockaddr_in name {
    };
    socklen_t namelen = sizeof(name);
    getsockname(sock, (struct sockaddr*)&name, &namelen);

    char buffer[100];
    const char* p = inet_ntop(AF_INET, &name.sin_addr, buffer, 100);

    if (p != nullptr) {
        MyIP = std::string(buffer);
    } else {
        MyIP = "0.0.0.0";
    }

    close(sock);

    return MyIP;
}

void SIGPIPE_handler(int /*s*/)
{
    DEBUG_COUT("Caught SIGPIPE");
}

[[noreturn]] void SIGSEGV_handler(int /*s*/)
{
    DEBUG_COUT("Caught SIGSEGV");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    exit(1);
}

[[noreturn]] void SIGTERM_handler(int /*s*/)
{
    DEBUG_COUT("Caught SIGTERM");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    exit(0);
}

[[noreturn]] void print_config_file_params_and_exit()
{
    static const std::string version = std::string(VESION_MAJOR) + "." + std::string(VESION_MINOR) + "." + std::string(GIT_COMMIT_HASH);
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
    std::set<std::pair<std::string, int>>& core_list);

void libevent(
    std::atomic<std::map<std::string, std::pair<uint, uint>>*>& statistics,
    std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& request_addreses,
    const std::string& host, int port,
    const std::string& network);

void sendStat(const std::string& network, std::string& host, int tx_port, BlockChainController& BlckChnCtrl);

int main(int argc, char** argv)
{
    //    signal(SIGPIPE, SIGPIPE_handler);
    signal(SIGTERM, SIGTERM_handler);
    signal(SIGSEGV, SIGSEGV_handler);

    std::string network;
    std::string host;
    int tx_port = 8181;
    std::string path;
    std::string known_hash;
    std::string key;
    std::set<std::pair<std::string, int>> core_list;

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

    ThreadPool TP;
    BlockChainController blockChainController(TP, key, path, known_hash, core_list, { host, tx_port }, skip_last_forging_and_state);

    std::thread(libevent, std::ref(blockChainController.get_wallet_statistics()), std::ref(blockChainController.get_wallet_request_addreses()), "wsstata.metahash.io", 80, "net-test").detach();
    std::thread(sendStat, std::ref(network), std::ref(host), tx_port, std::ref(blockChainController)).detach();

    BLOCK_SERVER BS(tx_port, [&blockChainController](const std::string_view req_post, const std::string_view req_url, const std::string_view req_sign, const std::string_view req_pubk) {
        if (req_url == "getinfo") {
            static const std::string version = std::string(VESION_MAJOR) + "." + std::string(VESION_MINOR) + "." + std::string(GIT_COUNT);
            rapidjson::StringBuffer s;
            rapidjson::Writer<rapidjson::StringBuffer> writer(s);
            writer.StartObject();
            {
                writer.String("result");
                writer.StartObject();
                {
                    writer.String("version");
                    writer.String(version.c_str());
                    writer.String("mh_addr");
                    writer.String(blockChainController.get_str_address().c_str());
                }
                writer.EndObject();
            }
            writer.EndObject();
            return std::string(s.GetString());
        } else {
            //        DEBUG_COUT(url_sw);
            return blockChainController.add_pack_to_queue(req_post, req_url, req_sign, req_pubk);
        }
    });
    BS.start();

    // http_server(TP, tx_port, [&blockChainController](HTTP_SERVER_IO* io_struct) {
    //     //        return "Hello World";
    //     std::string_view pack_sw(io_struct->req_post);

    //     std::string path = io_struct->req_path;
    //     path.erase(std::remove(path.begin(), path.end(), '/'), path.end());
    //     std::string_view url_sw(path);

    //     if (path == "getinfo") {
    //         static const std::string version = std::string(VESION_MAJOR) + "." + std::string(VESION_MINOR) + "." + std::string(GIT_COMMIT_HASH);
    //         rapidjson::StringBuffer s;
    //         rapidjson::Writer<rapidjson::StringBuffer> writer(s);
    //         writer.StartObject();
    //         {
    //             writer.String("result");
    //             writer.StartObject();
    //             {
    //                 writer.String("version");
    //                 writer.String(version.c_str());
    //                 writer.String("mh_addr");
    //                 writer.String(blockChainController.get_str_address().c_str());
    //             }
    //             writer.EndObject();
    //         }
    //         writer.EndObject();
    //         return std::string(s.GetString());
    //     } else {
    //         return blockChainController.add_pack_to_queue(pack_sw, url_sw);
    //     }
    // });
}

__attribute__((__noreturn__)) void sendStat(const std::string& network, std::string&, int, BlockChainController& BlckChnCtrl)
{
    CurlFetch CF("172.104.236.166", 5797);
    //    const std::string host_name = host + "_" + std::to_string(tx_port);
    const std::string version = std::string(VESION_MAJOR) + "." + std::string(VESION_MINOR) + "." + std::string(GIT_COUNT) + "." + std::string(GIT_COMMIT_HASH);

    bool forever = true;
    while (forever) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        std::string ip = getMyIp();
        uint64_t timestamp = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count();
        std::string req_post;

        {
            rapidjson::StringBuffer s;
            rapidjson::Writer<rapidjson::StringBuffer> writer(s);

            auto write_metric = [&writer](const std::string& key, const std::string& value) {
                writer.StartObject();
                {

                    writer.String("metric");
                    writer.String(key.c_str());

                    writer.String("type");
                    writer.String("none");

                    writer.String("value");
                    writer.String(value.c_str());
                }
                writer.EndObject();
            };

            writer.StartObject();
            {

                writer.String("params");
                writer.StartObject();
                {

                    writer.String("network");
                    writer.String(network.c_str());

                    writer.String("group");
                    writer.String("core");

                    writer.String("server");
                    writer.String(BlckChnCtrl.get_str_address().c_str());

                    writer.String("timestamp_ms");
                    writer.Uint64(timestamp);

                    writer.String("metrics");
                    writer.StartArray();
                    {
                        write_metric("ip", ip);
                        write_metric("mh_addr", BlckChnCtrl.get_str_address());
                        write_metric("last_block_info", "hash: " + BlckChnCtrl.get_last_block_str() + ";");
                        write_metric("version", version);
                    }
                    writer.EndArray();
                }
                writer.EndObject();
            }
            writer.EndObject();

            req_post = std::string(s.GetString());
        }

        std::string response;
        CF.post("save-metrics", req_post, response);
    }
    exit(0);
}

__attribute__((__noreturn__)) void libevent(
    std::atomic<std::map<std::string, std::pair<uint, uint>>*>& statistics,
    std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& request_addreses,
    const std::string& host, int port,
    const std::string& network)
{
    std::string key_type = "keys_tmh";
    if (network == "net-main") {
        key_type = "keys_mth";
    } else if (network == "net-dev") {
        key_type = "keys_tmh";
    } else if (network == "net-test") {
        key_type = "keys_mth";
    }

    //    mh::libevent::LibEvent levent;
    CurlFetch CF(host, port);
    bool endless_loop = true;
    while (endless_loop) {
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
            while (true) {
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

                        while (true) {
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
    std::set<std::pair<std::string, int>>& core_list)
{
    std::ifstream file(file_name);
    std::string line;

    /********************NETWORK NAME********************/
    if (std::getline(file, line)) {
        if (line != "net-main"
            && line != "net-dev"
            && line != "net-test") {

            DEBUG_COUT("Unknown network name:\t" + line);
            print_config_file_params_and_exit();
        }

        network = line;
        DEBUG_COUT("Network name:\t" + network);
    } else {
        DEBUG_COUT("Ivalid Configuration File");
        print_config_file_params_and_exit();
    }

    /********************LISTENING PORT********************/
    if (std::getline(file, line)) {
        std::stringstream linestream(line);
        linestream >> tx_host >> tx_port;

        if (tx_port <= 0) {
            DEBUG_COUT("Ivalid listening port:\t" + line);
            print_config_file_params_and_exit();
        }

        DEBUG_COUT("Listening host port:\t" + tx_host + "\t" + std::to_string(tx_port));
    } else {
        DEBUG_COUT("Ivalid Configuration File");
        print_config_file_params_and_exit();
    }

    /********************METACHAIN PATH********************/
    if (std::getline(file, line)) {
        path = line;

        if (std::filesystem::path dir(path); !std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
            DEBUG_COUT("Ivalid metachain path");
            print_config_file_params_and_exit();
        }

        DEBUG_COUT("Metachain path:\t" + path);
    } else {
        DEBUG_COUT("Ivalid Configuration File");
        print_config_file_params_and_exit();
    }

    /********************TRUSTED BLOCK HASH********************/
    if (std::getline(file, line)) {
        auto hash_as_vec = hex2bin(line);
        if (hash_as_vec.size() != 32) {
            DEBUG_COUT("Ivalid trusted block hash");
            print_config_file_params_and_exit();
        }

        hash = line;
        DEBUG_COUT("Trusted block hash:\t" + hash);
    } else {
        DEBUG_COUT("Ivalid Configuration File");
        print_config_file_params_and_exit();
    }

    /********************PRIVATE KEY********************/
    if (std::getline(file, line)) {
        std::vector<unsigned char> priv_k = hex2bin(line);
        std::vector<char> PubKey;
        if (!generate_public_key(PubKey, priv_k)) {
            DEBUG_COUT("Error while parsing Private key");
            print_config_file_params_and_exit();
        }

        std::array<char, 25> addres = get_address(PubKey);
        std::string Text_addres = "0x" + bin2hex(addres);
        DEBUG_COUT("got key for address:\t" + Text_addres);

        key = line;
    } else {
        DEBUG_COUT("Ivalid Configuration File");
        print_config_file_params_and_exit();
    }

    /********************KNOWN CORES********************/
    while (std::getline(file, line)) {
        std::stringstream linestream(line);
        std::string host;
        int port = 0;

        linestream >> host >> port;
        DEBUG_COUT("Core\t" + host + "\t" + std::to_string(port));

        if (port == 0) {
            break;
        }

        core_list.insert(std::make_pair(host, port));
    }
}