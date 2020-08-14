#include "controller.hpp"
//#include <meta_log.hpp>
#include <meta_constants.hpp>

namespace metahash::meta_core {

std::vector<char> ControllerImplementation::add_pack_to_queue(network::Request& request)
{
    auto roles = BC->check_addr(request.sender_mh_addr);
    auto url = request.request_type;
    auto pack = request.message;

    if (roles.count(META_ROLE_VERIF)) {
        switch (url) {
        case RPC_PING:
            dbg_RPC_PING ++;
            parse_S_PING(pack);
            return std::vector<char>();
        case RPC_TX:
            dbg_RPC_TX ++;
            parse_B_TX(pack);
            return std::vector<char>();
        case RPC_GET_CORE_LIST:
            dbg_RPC_GET_CORE_LIST ++;
            return parse_S_GET_CORE_LIST(pack);
        }
    }

    if (roles.count(META_ROLE_CORE)) {
        switch (url) {
        case RPC_APPROVE:
            dbg_RPC_APPROVE ++;
            parse_C_APPROVE(pack);
            return std::vector<char>();
        case RPC_DISAPPROVE:
            dbg_RPC_DISAPPROVE ++;
            parse_C_DISAPPROVE(pack);
            return std::vector<char>();
        case RPC_LAST_BLOCK:
            dbg_RPC_LAST_BLOCK ++;
            return parse_S_LAST_BLOCK(pack);
        case RPC_GET_BLOCK:
            dbg_RPC_GET_BLOCK ++;
            return parse_S_GET_BLOCK(pack);
        case RPC_GET_CHAIN:
            dbg_RPC_GET_CHAIN ++;
            return parse_S_GET_CHAIN(pack);
        case RPC_GET_CORE_LIST:
            dbg_RPC_GET_CORE_LIST ++;
            return parse_S_GET_CORE_LIST(pack);
        case RPC_CORE_LIST_APPROVE:
            dbg_RPC_CORE_LIST_APPROVE ++;
            parse_S_CORE_LIST_APPROVE(request.sender_mh_addr, pack);
            return std::vector<char>();
        }
    }

    if (request.sender_mh_addr == current_cores[0]) {
        if (url == RPC_PRETEND_BLOCK) {
            dbg_RPC_PRETEND_BLOCK ++;
            parse_C_PRETEND_BLOCK(pack);
            return std::vector<char>();
        }
    }

    /*{
        DEBUG_COUT(request.request_id);
        DEBUG_COUT(crypto::int2hex(request.request_type));
        DEBUG_COUT(request.sender_mh_addr);
        DEBUG_COUT(request.remote_ip_address);
        DEBUG_COUT(request.message.size());
        for (const auto& role : roles) {
            DEBUG_COUT(role);
        }
    }*/
    dbg_RPC_NONE ++;

    return std::vector<char>();
}

}