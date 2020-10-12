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

    switch (url) {
    case RPC_APPROVE:
        dbg_RPC_APPROVE++;
        parse_RPC_APPROVE(pack);
        return std::vector<char>();
    case RPC_DISAPPROVE:
        dbg_RPC_DISAPPROVE++;
        parse_RPC_DISAPPROVE(pack);
        return std::vector<char>();
    case RPC_GET_APPROVE:
        dbg_RPC_GET_APPROVE++;
        return parse_RPC_GET_APPROVE(pack);
    case RPC_LAST_BLOCK:
        dbg_RPC_LAST_BLOCK++;
        return parse_RPC_LAST_BLOCK(pack);
    case RPC_GET_BLOCK:
        dbg_RPC_GET_BLOCK++;
        return parse_RPC_GET_BLOCK(pack);
    case RPC_GET_CHAIN:
        dbg_RPC_GET_CHAIN++;
        return parse_RPC_GET_CHAIN(pack);
    case RPC_GET_MISSING_BLOCK_LIST:
        dbg_RPC_GET_MISSING_BLOCK_LIST++;
        return parse_RPC_GET_MISSING_BLOCK_LIST(pack);
    case RPC_GET_CORE_LIST:
        dbg_RPC_GET_CORE_LIST++;
        return parse_RPC_GET_CORE_LIST(pack);
    }

    if (roles.count(META_ROLE_VERIF)) {
        if (url == RPC_TX) {
            dbg_RPC_TX++;
            parse_RPC_TX(pack);
            return std::vector<char>();
        }
    }

    if (roles.count(META_ROLE_CORE)) {
        if (url == RPC_CORE_LIST_APPROVE) {
            dbg_RPC_CORE_LIST_APPROVE++;
            parse_RPC_CORE_LIST_APPROVE(request.sender_mh_addr, pack);
            return std::vector<char>();
        }
    }

    if (!current_cores.empty() && request.sender_mh_addr == current_cores[0]) {
        if (url == RPC_PRETEND_BLOCK) {
            dbg_RPC_PRETEND_BLOCK++;
            parse_RPC_PRETEND_BLOCK(pack);
            return std::vector<char>();
        }
    }

    dbg_RPC_NONE++;

    return std::vector<char>();
}

void ControllerImplementation::log_network_statistics(uint64_t timestamp)
{
    /// DEBUG INFO BEGIN
    if (timestamp > dbg_timestamp + 60) {
        DEBUG_COUT("Ping\tTX\tGetCL\tAPPROVE\tDisAprv\tGetAprv\tLastBlo\tGetBlok\tGetChai\tGetBLst\tCoreLst\tPRETEND\tNone");
        DEBUG_COUT(std::to_string(dbg_RPC_PING) + "\t"
            + std::to_string(dbg_RPC_TX) + "\t"
            + std::to_string(dbg_RPC_GET_CORE_LIST) + "\t"
            + std::to_string(dbg_RPC_APPROVE) + "\t"
            + std::to_string(dbg_RPC_DISAPPROVE) + "\t"
            + std::to_string(dbg_RPC_GET_APPROVE) + "\t"
            + std::to_string(dbg_RPC_LAST_BLOCK) + "\t"
            + std::to_string(dbg_RPC_GET_BLOCK) + "\t"
            + std::to_string(dbg_RPC_GET_CHAIN) + "\t"
            + std::to_string(dbg_RPC_GET_MISSING_BLOCK_LIST) + "\t"
            + std::to_string(dbg_RPC_CORE_LIST_APPROVE) + "\t"
            + std::to_string(dbg_RPC_PRETEND_BLOCK) + "\t"
            + std::to_string(dbg_RPC_NONE));

        dbg_RPC_PING = 0;
        dbg_RPC_TX = 0;
        dbg_RPC_GET_CORE_LIST = 0;
        dbg_RPC_APPROVE = 0;
        dbg_RPC_DISAPPROVE = 0;
        dbg_RPC_GET_APPROVE = 0;
        dbg_RPC_LAST_BLOCK = 0;
        dbg_RPC_GET_BLOCK = 0;
        dbg_RPC_GET_CHAIN = 0;
        dbg_RPC_GET_MISSING_BLOCK_LIST = 0;
        dbg_RPC_CORE_LIST_APPROVE = 0;
        dbg_RPC_PRETEND_BLOCK = 0;
        dbg_RPC_NONE = 0;

        dbg_timestamp = timestamp;
    }
    /// DEBUG INFO END
}

}