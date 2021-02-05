#include "controller.hpp"

#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_core {

std::vector<char> ControllerImplementation::add_pack_to_queue(network::Request& request)
{
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
    };

    auto&& stat = get_stat_iter()->second;

    switch (url) {
    case RPC_TX:
        if (roles.count(META_ROLE_VERIF)) {
            stat.dbg_RPC_TX++;
            parse_RPC_TX(pack);
            return std::vector<char>();
        }
        break;
    case RPC_APPROVE:
        stat.dbg_RPC_APPROVE++;
        parse_RPC_APPROVE(pack);
        return std::vector<char>();
    case RPC_DISAPPROVE:
        stat.dbg_RPC_DISAPPROVE++;
        parse_RPC_DISAPPROVE(pack);
        return std::vector<char>();
    case RPC_GET_APPROVE:
        stat.dbg_RPC_GET_APPROVE++;
        return parse_RPC_GET_APPROVE(pack);
    case RPC_LAST_BLOCK:
        stat.dbg_RPC_LAST_BLOCK++;
        return parse_RPC_LAST_BLOCK(pack);
    case RPC_GET_BLOCK:
        stat.dbg_RPC_GET_BLOCK++;
        return parse_RPC_GET_BLOCK(pack);
    case RPC_GET_CHAIN:
        stat.dbg_RPC_GET_CHAIN++;
        return parse_RPC_GET_CHAIN(pack);
    case RPC_GET_MISSING_BLOCK_LIST:
        stat.dbg_RPC_GET_MISSING_BLOCK_LIST++;
        return parse_RPC_GET_MISSING_BLOCK_LIST(pack);
    case RPC_GET_CORE_LIST:
        stat.dbg_RPC_GET_CORE_LIST++;
        return parse_RPC_GET_CORE_LIST(pack);
    case RPC_CORE_LIST_APPROVE:
        if (roles.count(META_ROLE_CORE)) {
            stat.dbg_RPC_CORE_LIST_APPROVE++;
            parse_RPC_CORE_LIST_APPROVE(request.sender_mh_addr, pack);
            return std::vector<char>();
        }
        break;
    case RPC_PRETEND_BLOCK:
        if (!current_cores.empty() && request.sender_mh_addr == current_cores[0]) {
            stat.dbg_RPC_PRETEND_BLOCK++;
            parse_RPC_PRETEND_BLOCK(pack);
            return std::vector<char>();
        }
        break;
    default:
        stat.dbg_RPC_NONE++;
        break;
    }

    return std::vector<char>();
}

void ControllerImplementation::log_network_statistics(uint64_t timestamp)
{
    /// DEBUG INFO BEGIN
    if (timestamp > dbg_timestamp + 60) {
        DEBUG_COUT("\t0x00112233445566778899aabbccddeeffgghhffjjiikkllmmnn\tTX\tGetCL\tApprove\tDisAprv\tGetAprv\tLastBlo\tGetBlok\tGetChai\tGetBLst\tCoreLst\tPretend\tNone\tRoles");

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

            DEBUG_COUT(addr + "\t"
                + std::to_string(stat.dbg_RPC_TX) + "\t"
                + std::to_string(stat.dbg_RPC_GET_CORE_LIST) + "\t"
                + std::to_string(stat.dbg_RPC_APPROVE) + "\t"
                + std::to_string(stat.dbg_RPC_DISAPPROVE) + "\t"
                + std::to_string(stat.dbg_RPC_GET_APPROVE) + "\t"
                + std::to_string(stat.dbg_RPC_LAST_BLOCK) + "\t"
                + std::to_string(stat.dbg_RPC_GET_BLOCK) + "\t"
                + std::to_string(stat.dbg_RPC_GET_CHAIN) + "\t"
                + std::to_string(stat.dbg_RPC_GET_MISSING_BLOCK_LIST) + "\t"
                + std::to_string(stat.dbg_RPC_CORE_LIST_APPROVE) + "\t"
                + std::to_string(stat.dbg_RPC_PRETEND_BLOCK) + "\t"
                + std::to_string(stat.dbg_RPC_NONE) + "\t"
                + role_list);

            stat.dbg_RPC_TX = 0;
            stat.dbg_RPC_GET_CORE_LIST = 0;
            stat.dbg_RPC_APPROVE = 0;
            stat.dbg_RPC_DISAPPROVE = 0;
            stat.dbg_RPC_GET_APPROVE = 0;
            stat.dbg_RPC_LAST_BLOCK = 0;
            stat.dbg_RPC_GET_BLOCK = 0;
            stat.dbg_RPC_GET_CHAIN = 0;
            stat.dbg_RPC_GET_MISSING_BLOCK_LIST = 0;
            stat.dbg_RPC_CORE_LIST_APPROVE = 0;
            stat.dbg_RPC_PRETEND_BLOCK = 0;
            stat.dbg_RPC_NONE = 0;
        }

        dbg_timestamp = timestamp;
    }
    /// DEBUG INFO END
}

}