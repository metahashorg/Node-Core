#include "controller.hpp"

#include "block.h"
#include "chain.h"

#include <meta_log.hpp>
#include <statics.hpp>

namespace metahash::metachain {

std::vector<char> ControllerImplementation::add_pack_to_queue(net_io::Request& request)
{
    auto roles = BC->check_addr(request.sender_mh_addr);
    auto url = request.request_type;
    auto pack = request.message;

    if (roles.count(META_ROLE_VERIF)) {
        switch (url) {
        case RPC_PING:
            parse_S_PING(pack);
            return std::vector<char>();
        case RPC_TX:
            parse_B_TX(pack);
            return std::vector<char>();
        case RPC_GET_CORE_LIST:
            return parse_S_GET_CORE_LIST(pack);
        }
    }

    if (roles.count(META_ROLE_CORE)) {
        switch (url) {
        case RPC_APPROVE:
            parse_C_APPROVE(pack);
            return std::vector<char>();
        case RPC_DISAPPROVE:
            parse_C_DISAPPROVE(pack);
            return std::vector<char>();
        case RPC_LAST_BLOCK:
            return parse_S_LAST_BLOCK(pack);
        case RPC_GET_BLOCK:
            return parse_S_GET_BLOCK(pack);
        case RPC_GET_CHAIN:
            return parse_S_GET_CHAIN(pack);
        case RPC_GET_CORE_LIST:
            return parse_S_GET_CORE_LIST(pack);
        case RPC_CORE_LIST_APPROVE:
            parse_S_CORE_LIST_APPROVE(request.sender_mh_addr, pack);
            return std::vector<char>();
        }
    }

    if (request.sender_mh_addr == current_cores[0]) {
        if (url == RPC_PRETEND_BLOCK) {
            parse_C_PRETEND_BLOCK(pack);
            return std::vector<char>();
        }
    }

    {
        DEBUG_COUT(request.request_id);
        DEBUG_COUT(crypto::int2hex(request.request_type));
        DEBUG_COUT(request.sender_mh_addr);
        DEBUG_COUT(request.remote_ip_address);
        DEBUG_COUT(request.message.size());
        for (const auto& role : roles) {
            DEBUG_COUT(role);
        }
    }

    return std::vector<char>();
}

void ControllerImplementation::parse_S_PING(std::string_view)
{
}

void ControllerImplementation::parse_B_TX(std::string_view pack)
{
    auto* tx_list = new std::list<TX*>();

    uint64_t index = 0;
    uint64_t tx_size;
    std::string_view tx_size_arr(&pack[index], pack.size() - index);
    uint64_t varint_size = crypto::read_varint(tx_size, tx_size_arr);
    if (varint_size < 1) {
        DEBUG_COUT("corrupt varint size");
        return;
    }
    index += varint_size;

    while (tx_size > 0) {
        if (index + tx_size >= pack.size()) {
            DEBUG_COUT("corrupt tx size");
            return;
        }
        std::string_view tx_sw(&pack[index], tx_size);
        index += tx_size;

        TX* p_tx = new TX;
        if (p_tx->parse(tx_sw)) {
            tx_list->push_back(p_tx);
        } else {
            delete p_tx;
            DEBUG_COUT("corrupt tx");
        }

        tx_size_arr = std::string_view(&pack[index], pack.size() - index);
        varint_size = crypto::read_varint(tx_size, tx_size_arr);
        if (varint_size < 1) {
            DEBUG_COUT("corrupt varint size");
            return;
        }
        index += varint_size;
    }

    if (tx_list->empty()) {
        delete tx_list;
    } else {
        serial_execution.post([this, tx_list] {
            transactions.insert(transactions.end(), tx_list->begin(), tx_list->end());
            delete tx_list;
        });
    }
}

void ControllerImplementation::parse_C_PRETEND_BLOCK(std::string_view pack)
{
    std::string_view block_sw(pack);
    Block* block = parse_block(block_sw);

    if (block) {
        if (dynamic_cast<CommonBlock*>(block)) {
            serial_execution.post([this, block] {
                if (blocks.find(block->get_prev_hash()) == blocks.end()) {
                    serial_execution.post([this] {
                        actualize_chain();
                    });
                }

                if (!blocks.insert({ block->get_block_hash(), block }).second) {
                    delete block;
                }
            });
        } else if (dynamic_cast<RejectedTXBlock*>(block)) {
            serial_execution.post([this, block] {
                write_block(block);
            });
        } else {
            delete block;
        }
    }
}

void ControllerImplementation::parse_C_APPROVE(std::string_view pack)
{
    std::string_view approve_sw(pack);
    auto* p_ar = new ApproveRecord;
    if (p_ar->parse(approve_sw)) {
        p_ar->approve = true;

        serial_execution.post([this, p_ar] {
            apply_approve(p_ar);
        });
    } else {
        delete p_ar;
    }
}

void ControllerImplementation::parse_C_DISAPPROVE(std::string_view pack)
{
    std::string_view approve_sw(pack);
    auto* p_ar = new ApproveRecord;
    if (p_ar->parse(approve_sw)) {
        p_ar->approve = false;

        serial_execution.post([this, p_ar] {
            apply_approve(p_ar);
        });
    } else {
        delete p_ar;
    }
}

std::vector<char> ControllerImplementation::parse_S_LAST_BLOCK(std::string_view)
{
    std::vector<char> last_block;
    last_block.insert(last_block.end(), last_applied_block.begin(), last_applied_block.end());
    char* p_timestamp = reinterpret_cast<char*>(&prev_timestamp);
    last_block.insert(last_block.end(), p_timestamp, p_timestamp + 8);

    return last_block;
}

std::vector<char> ControllerImplementation::parse_S_GET_BLOCK(std::string_view pack)
{
    if (pack.size() < 32) {
        DEBUG_COUT("pack.size() < 32");
        DEBUG_COUT(crypto::bin2hex(pack));
        return std::vector<char>();
    }

    sha256_2 block_hash;
    std::copy_n(pack.begin(), 32, block_hash.begin());

    if (blocks.find(block_hash) != blocks.end()) {
        DEBUG_COUT("blocks[block_hash]->get_data().size()\t" + std::to_string(blocks[block_hash]->get_data().size()));
        return blocks[block_hash]->get_data();
    } else {
        DEBUG_COUT("blocks.find(block_hash) != blocks.end()");
        DEBUG_COUT(crypto::bin2hex(block_hash));
    }

    return std::vector<char>();
}

std::vector<char> ControllerImplementation::parse_S_GET_CHAIN(std::string_view pack)
{
    sha256_2 prev_block = { { 0 } };
    //    DEBUG_COUT(std::to_string(pack.size()));

    if (pack.size() < 32) {
        return std::vector<char>();
    }

    std::copy_n(pack.begin(), 32, prev_block.begin());

    std::vector<char> chain;
    sha256_2 got_block = master() ? last_created_block : last_applied_block;

    //    DEBUG_COUT(bin2hex(prev_block));
    //    DEBUG_COUT(bin2hex(got_block));

    while (got_block != prev_block && blocks.find(got_block) != blocks.end()) {
        auto& block_data = blocks[got_block]->get_data();

        uint64_t block_size = block_data.size();
        chain.insert(chain.end(), reinterpret_cast<char*>(&block_size), reinterpret_cast<char*>(&block_size) + sizeof(uint64_t));
        chain.insert(chain.end(), block_data.begin(), block_data.end());

        got_block = blocks[got_block]->get_prev_hash();
    }

    return chain;
}

std::vector<char> ControllerImplementation::parse_S_GET_CORE_LIST(std::string_view pack)
{
    cores.add_cores(pack);

    return cores.get_core_list();
}

void ControllerImplementation::parse_S_CORE_LIST_APPROVE(std::string core, std::string_view pack)
{
    uint64_t current_timestamp = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count());
    uint64_t current_generation = current_timestamp / CORE_LIST_RENEW_PERIOD;
    std::string list(pack);

    serial_execution.post([this, core, list, current_generation] {
        proposed_cores[current_generation][list].insert(core);
    });
}

}