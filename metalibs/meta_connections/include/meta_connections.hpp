#ifndef META_CONNECTIONS_HPP
#define META_CONNECTIONS_HPP

#include <map>
#include <set>
#include <shared_mutex>

#include <meta_client.h>

namespace metahash::connection {

class MetaConnection {
private:
    const uint64_t concurrent_connections_count = 2;

    std::shared_mutex core_lock;
    std::map<std::string, network::meta_client*> cores;

    boost::asio::io_context& io_context;
    std::string my_host;
    int my_port;

    crypto::Signer& signer;

    const bool spectator = false;

public:
    MetaConnection(boost::asio::io_context& io_context, const std::string&, int, crypto::Signer&, bool spectator = false);

    void init(const std::map<std::string, std::pair<std::string, int>>& core_list);

    void sync_core_lists();
    void add_new_cores(const std::set<std::tuple<std::string, std::string, int>>& hosts);

    void add_cores(std::string_view pack);
    std::vector<char> get_core_list();

    void send_no_return(uint64_t req_type, const std::vector<char>& req_data);
    std::map<std::string, std::vector<char>> send_with_return(uint64_t req_type, const std::vector<char>& req_data);

    void send_no_return_to_core(const std::string& addr, uint64_t req_type, const std::vector<char>& req_data);
    std::vector<char> send_with_return_to_core(const std::string& addr, uint64_t req_type, const std::vector<char>& req_data);

    bool online(const std::string& addr);
    std::set<std::string> get_online_cores();
};

}

#endif // META_CONNECTIONS_HPP
