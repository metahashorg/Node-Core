#include "controller.hpp"

#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_core {

std::vector<char> ControllerImplementation::add_pack_to_queue(network::Request& request)
{
    static const std::vector<char> empty_resp;

    const auto& sender_addr = request.sender_mh_addr;
    auto roles = BC.check_addr(sender_addr);
    const auto url = request.request_type;
    const auto pack = request.message;

    auto get_stat_iter = [this, sender_addr] {
        bool need_insertion = false;
        {
            std::shared_lock lock(income_nodes_stat_lock);
            auto stat_iter = income_nodes_stat.find(sender_addr);
            if (stat_iter == income_nodes_stat.end()) {
                need_insertion = true;
            } else {
                return stat_iter;
            }
        }
        if (need_insertion) {
            std::unique_lock lock(income_nodes_stat_lock);
            income_nodes_stat[sender_addr];
            return income_nodes_stat.find(sender_addr);
        }
        return income_nodes_stat.end();
    };

    auto&& stat = get_stat_iter()->second;

    {
        bool need_insertion = false;
        auto& sender_ip = request.remote_ip_address;
        {
            std::shared_lock lock(stat.ip_lock);
            auto ip_iter = stat.ip.find(sender_ip);
            if (ip_iter == stat.ip.end()) {
                need_insertion = true;
            }
        }
        if (need_insertion) {
            std::unique_lock lock(stat.ip_lock);
            stat.ip.insert(sender_ip);
        }
    }

    switch (url) {
    case RPC_TX:
        if (roles.count(META_ROLE_VERIF)) {
            stat.dbg_RPC_TX++;
            parse_RPC_TX(pack);
            return empty_resp;
        }
        break;
    case RPC_APPROVE:
        if (roles.count(META_ROLE_CORE)) {
            stat.dbg_RPC_APPROVE++;
            parse_RPC_APPROVE(pack);
            return empty_resp;
        }
        break;
    case RPC_DISAPPROVE:
        if (roles.count(META_ROLE_CORE)) {
            stat.dbg_RPC_DISAPPROVE++;
            parse_RPC_DISAPPROVE(pack);
            return empty_resp;
        }
        break;
    case RPC_GET_APPROVE:
        stat.dbg_RPC_GET_APPROVE++;
        parse_RPC_GET_APPROVE(request.sender_mh_addr, pack);
        return empty_resp;
        break;
    case RPC_APPROVE_LIST:
        stat.dbg_RPC_APPROVE_LIST++;
        parse_RPC_APPROVE_LIST(pack);
        return empty_resp;
        break;
    case RPC_LAST_BLOCK:
        stat.dbg_RPC_LAST_BLOCK++;
        return parse_RPC_LAST_BLOCK(pack);
        break;
    case RPC_GET_BLOCK:
        stat.dbg_RPC_GET_BLOCK++;
        return parse_RPC_GET_BLOCK(pack);
        break;
    case RPC_GET_MISSING_BLOCK_LIST:
        stat.dbg_RPC_GET_MISSING_BLOCK_LIST++;
        return parse_RPC_GET_MISSING_BLOCK_LIST(pack);
        break;
    case RPC_GET_CORE_LIST:
        stat.dbg_RPC_GET_CORE_LIST++;
        return parse_RPC_GET_CORE_LIST(pack);
    case RPC_CORE_LIST_APPROVE:
        if (roles.count(META_ROLE_CORE)) {
            stat.dbg_RPC_CORE_LIST_APPROVE++;
            parse_RPC_CORE_LIST_APPROVE(request.sender_mh_addr, pack);
            return empty_resp;
        }
        break;
    case RPC_PRETEND_BLOCK:
        if (roles.count(META_ROLE_CORE)) {
            stat.dbg_RPC_PRETEND_BLOCK++;
            parse_RPC_PRETEND_BLOCK(pack);
            return empty_resp;
        }
        break;
    default:
        break;
    }

    stat.dbg_RPC_NONE++;
    return empty_resp;
}

void ControllerImplementation::log_network_statistics(uint64_t timestamp)
{
    /// DEBUG INFO BEGIN
    if (timestamp > dbg_timestamp + 60) {
        std::string log_msg = "\n";
        log_msg += "0x00112233445566778899aabbccddeeffgghhffjjiikkllmmnn\tTX\tGetCL\tApprove\tDisAprv\tGetAprv\tAprvLst\tLastBlo\tGetBlok\tGetChai\tGetBLst\tCoreLst\tPretend\tNone\tRoles\tIP";
        log_msg += "\n";

        {
            std::shared_lock lock(income_nodes_stat_lock);
            for (auto&& node_stat : income_nodes_stat) {
                auto&& stat = node_stat.second;
                auto&& addr = node_stat.first;

                std::string role_list;
                {
                    auto roles = BC.check_addr(addr);
                    for (const auto& role : roles) {
                        role_list += role + "\t";
                    }
                }

                std::string ip_list;
                {
                    std::shared_lock lock(stat.ip_lock);
                    for (const auto& ip : stat.ip) {
                        ip_list += ip + "\t";
                    }
                }

                auto total_stat = stat.dbg_RPC_TX
                    + stat.dbg_RPC_GET_CORE_LIST
                    + stat.dbg_RPC_APPROVE
                    + stat.dbg_RPC_DISAPPROVE
                    + stat.dbg_RPC_GET_APPROVE
                    + stat.dbg_RPC_APPROVE_LIST
                    + stat.dbg_RPC_LAST_BLOCK
                    + stat.dbg_RPC_GET_BLOCK
                    + stat.dbg_RPC_GET_CHAIN
                    + stat.dbg_RPC_GET_MISSING_BLOCK_LIST
                    + stat.dbg_RPC_CORE_LIST_APPROVE
                    + stat.dbg_RPC_PRETEND_BLOCK
                    + stat.dbg_RPC_NONE;

                if (total_stat == 0 && ip_list.empty()) {
                    continue;
                }

                log_msg += addr + "\t"
                    + std::to_string(stat.dbg_RPC_TX) + "\t"
                    + std::to_string(stat.dbg_RPC_GET_CORE_LIST) + "\t"
                    + std::to_string(stat.dbg_RPC_APPROVE) + "\t"
                    + std::to_string(stat.dbg_RPC_DISAPPROVE) + "\t"
                    + std::to_string(stat.dbg_RPC_GET_APPROVE) + "\t"
                    + std::to_string(stat.dbg_RPC_APPROVE_LIST) + "\t"
                    + std::to_string(stat.dbg_RPC_LAST_BLOCK) + "\t"
                    + std::to_string(stat.dbg_RPC_GET_BLOCK) + "\t"
                    + std::to_string(stat.dbg_RPC_GET_CHAIN) + "\t"
                    + std::to_string(stat.dbg_RPC_GET_MISSING_BLOCK_LIST) + "\t"
                    + std::to_string(stat.dbg_RPC_CORE_LIST_APPROVE) + "\t"
                    + std::to_string(stat.dbg_RPC_PRETEND_BLOCK) + "\t"
                    + std::to_string(stat.dbg_RPC_NONE) + "\t"
                    + role_list
                    + ip_list;
                log_msg += "\n";

                stat.dbg_RPC_TX = 0;
                stat.dbg_RPC_GET_CORE_LIST = 0;
                stat.dbg_RPC_APPROVE = 0;
                stat.dbg_RPC_DISAPPROVE = 0;
                stat.dbg_RPC_GET_APPROVE = 0;
                stat.dbg_RPC_APPROVE_LIST = 0;
                stat.dbg_RPC_LAST_BLOCK = 0;
                stat.dbg_RPC_GET_BLOCK = 0;
                stat.dbg_RPC_GET_CHAIN = 0;
                stat.dbg_RPC_GET_MISSING_BLOCK_LIST = 0;
                stat.dbg_RPC_CORE_LIST_APPROVE = 0;
                stat.dbg_RPC_PRETEND_BLOCK = 0;
                stat.dbg_RPC_NONE = 0;

                {
                    std::unique_lock lock(stat.ip_lock);
                    stat.ip.clear();
                }
            }
        }

        dbg_timestamp = timestamp;

        DEBUG_COUT(log_msg);
    }
    /// DEBUG INFO END
}

}