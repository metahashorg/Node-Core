#include "core_service.h"

#include <curl_pp.hpp>
#include <meta_constants.hpp>
#include <meta_log.hpp>
#include <rapidjson/document.h>

#include <set>

struct sync_stat {
    boost::asio::io_context& ioc;
    std::atomic<std::map<std::string, std::pair<uint, uint>>*>& statistics;
    std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& request_addresses;
    const std::string host;
    const std::string port;
    const std::string key_type;

    boost::asio::deadline_timer timer;

    sync_stat(
        boost::asio::io_context& ioc,
        std::atomic<std::map<std::string, std::pair<uint, uint>>*>& statistics,
        std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& request_addresses,
        const std::string& host, int port,
        const std::string& network)
        : ioc(ioc)
        , statistics(statistics)
        , request_addresses(request_addresses)
        , host(host)
        , port(std::to_string(port))
        , key_type(network == "net-dev" ? "keys_tmh" : "keys_mth")
        ,timer(ioc, boost::posix_time::minutes(15))
    {
    }

    std::deque<std::pair<std::string, uint64_t>>* get_addresses()
    {
        std::deque<std::pair<std::string, uint64_t>>* p_addresses;
        while (true) {
            std::deque<std::pair<std::string, uint64_t>>* null_adr = nullptr;
            p_addresses = request_addresses.load();
            if (request_addresses.compare_exchange_strong(p_addresses, null_adr)) {
                break;
            }
        }

        return p_addresses;
    }

    std::string make_request_string(const std::deque<std::pair<std::string, uint64_t>>& addresses)
    {
        bool first = true;
        std::string request_string;
        request_string += R"({"app":"CoreServiceIP","data":{")" + key_type + "\":[";
        for (const auto& addr : addresses) {
            if (first) {
                first = false;
            } else {
                request_string += ",";
            }
            request_string += "\"" + addr.first + "\"";
        }
        request_string += "]}}";

        return request_string;
    }

    std::map<std::string, std::map<std::string, std::tuple<uint64_t, uint64_t, uint64_t>>> parse_response(const std::string& response) {
        std::map<std::string, std::map<std::string, std::tuple<uint64_t, uint64_t, uint64_t>>> addr_stat_map;

        rapidjson::Document addr_stat;
        if (!addr_stat.Parse(response.c_str()).HasParseError()) {
            if (addr_stat.HasMember("data") && addr_stat["data"].IsObject()) {
                const auto& data = addr_stat["data"];

                if (data.HasMember(key_type.c_str()) && data[key_type.c_str()].IsArray()) {
                    const auto& addrs = data[key_type.c_str()];


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
                }
            }
        } else {
            DEBUG_COUT("error in service response");
            DEBUG_COUT(response);
        }

        return addr_stat_map;
    }

    void perform() {
        if (auto* p_addresses = get_addresses(); p_addresses != nullptr) {
            std::deque<std::pair<std::string, uint64_t>> addresses(*p_addresses);

            {
                std::deque<std::pair<std::string, uint64_t>>* null_adr = nullptr;
                if (!request_addresses.compare_exchange_strong(null_adr, p_addresses)) {
                    delete p_addresses;
                }
            }

            std::string request_string = make_request_string(addresses);

            DEBUG_COUT(std::to_string(request_string.size()));

            http::client::send_message_with_callback(ioc, host, port, "/", request_string, [this, addresses](bool success, std::string response) {
                if (success) {
                    DEBUG_COUT(std::to_string(response.size()));

                    std::map<std::string, std::map<std::string, std::tuple<uint64_t, uint64_t, uint64_t>>> addr_stat_map = parse_response(response);

                    if (!addr_stat_map.empty()) {
                        std::set<uint64_t> ips;
                        std::set<std::string> hwids_ticket;
                        std::set<std::string> hwids_online;
                        auto* p_got_statistics = new std::map<std::string, std::pair<uint, uint>>();
                        auto& got_statistics = *p_got_statistics;

                        for (const auto& addr_pair : addresses) {
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

                    timer = boost::asio::deadline_timer(ioc, boost::posix_time::minutes(15));
                    timer.async_wait([this](const boost::system::error_code&) {
                        perform();
                    });
                } else {
                    DEBUG_COUT(response);
                    perform();
                }
            });

        } else {
            DEBUG_COUT("no adr list");
            timer = boost::asio::deadline_timer(ioc, boost::posix_time::minutes(15));
            timer.async_wait([this](const boost::system::error_code&) {
                perform();
            });
        }
    }
};

void libevent(
    boost::asio::io_context& ioc,
    std::atomic<std::map<std::string, std::pair<uint, uint>>*>& statistics,
    std::atomic<std::deque<std::pair<std::string, uint64_t>>*>& request_addreses,
    const std::string& host, int port,
    const std::string& network) {

    auto* sstat = new sync_stat(ioc, statistics, request_addreses, host, port, network);
    sstat->perform();
}