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
    std::string_view pack(request.message.data(), request.message.size());

    if (roles.find(META_ROLE_VERIF) != roles.end()) {
        if (url == RPC_PING) {
            DEBUG_COUT("RPC_PING");
            parse_S_PING(pack);
        } else if (url == RPC_TX && master()) {
            DEBUG_COUT("RPC_TX");
            parse_B_TX(pack);
        }
    }

    if (roles.find(META_ROLE_CORE) != roles.end()) {
        switch (url) {
        case RPC_APPROVE:
            DEBUG_COUT("RPC_APPROVE");
            parse_C_APPROVE(pack);
            break;
        case RPC_DISAPPROVE:
            DEBUG_COUT("RPC_DISAPPROVE");
            parse_C_DISAPPROVE(pack);
            break;
        case RPC_APPROVE_BLOCK:
            DEBUG_COUT("RPC_APPROVE_BLOCK");
            parse_C_APPROVE_BLOCK(pack);
            break;
        case RPC_LAST_BLOCK:
            DEBUG_COUT("RPC_LAST_BLOCK");
            return parse_S_LAST_BLOCK(pack);
            break;
        case RPC_GET_BLOCK:
            DEBUG_COUT("RPC_GET_BLOCK");
            return parse_S_GET_BLOCK(pack);
            break;
        case RPC_GET_CHAIN:
            DEBUG_COUT("RPC_GET_CHAIN");
            return parse_S_GET_CHAIN(pack);
            break;
        case RPC_GET_CORE_LIST:
            DEBUG_COUT("RPC_GET_CORE_LIST");
            return parse_S_GET_CORE_LIST(pack);
            break;
        case RPC_GET_CORE_ADDR:
            DEBUG_COUT("RPC_GET_CORE_ADDR");
            return parse_S_GET_CORE_ADDR(pack);
            break;
        }
    }

    if (roles.find(META_ROLE_MASTER) != roles.end()) {
        if (url == RPC_PRETEND_BLOCK) {
            DEBUG_COUT("RPC_PRETEND_BLOCK");
            parse_C_PRETEND_BLOCK(pack);
        }
    }

    return std::vector<char>();
}

void ControllerImplementation::parse_S_PING(std::string_view)
{
}

void ControllerImplementation::parse_B_TX(std::string_view pack)
{
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
            tx_queue.enqueue(p_tx);
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
}

void ControllerImplementation::parse_C_APPROVE_BLOCK(std::string_view pack)
{
    uint64_t block_size = 0;
    uint64_t approve_size = 0;

    uint64_t offset = crypto::read_varint(approve_size, pack);
    if (!offset) {
        DEBUG_COUT("corrupt pack");
    }

    std::string_view approve_sw(pack.data() + offset, approve_size);
    auto* p_ar = new ApproveRecord;
    if (p_ar->parse(approve_sw)) {
        p_ar->approve = true;
        approve_queue.enqueue(p_ar);
    } else {
        DEBUG_COUT("corrupt pack");
        delete p_ar;
    }

    std::string_view block_raw(pack.data() + offset + approve_size, pack.size() - (offset + approve_size));

    offset = crypto::read_varint(block_size, block_raw);
    if (!offset) {
        DEBUG_COUT("corrupt pack");
    }
    std::string_view block_sw(block_raw.data() + offset, block_size);
    Block* block = parse_block(block_sw);

    if (block) {
        block_queue.enqueue(block);
    } else {
        DEBUG_COUT("corrupt pack");
    }
}

void ControllerImplementation::parse_C_PRETEND_BLOCK(std::string_view pack)
{
    std::string_view block_sw(pack);
    Block* block = parse_block(block_sw);

    if (block) {
        if (dynamic_cast<CommonBlock*>(block) || dynamic_cast<RejectedTXBlock*>(block)) {
            block_queue.enqueue(block);
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
        approve_queue.enqueue(p_ar);
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
        approve_queue.enqueue(p_ar);
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

    DEBUG_COUT("parse_S_LAST_BLOCK");
    DEBUG_COUT(prev_timestamp);
    DEBUG_COUT(crypto::bin2hex(last_applied_block));

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

std::vector<char> ControllerImplementation::parse_S_GET_CORE_LIST(std::string_view)
{
    return cores.get_core_list();
}

std::vector<char> ControllerImplementation::parse_S_GET_CORE_ADDR(std::string_view pack)
{
    std::string addr_req_str(pack);
    std::stringstream ss(addr_req_str);
    std::string item;
    std::vector<std::string> elems;
    while (std::getline(ss, item, ':')) {
        elems.push_back(std::move(item));
    }

    if (elems.size() == 3) {
        cores.add_core(elems[0], elems[1], std::stoi(elems[2]));
    }

    auto mh_addr = signer.get_mh_addr();
    std::vector<char> addr_as_vector;
    addr_as_vector.insert(addr_as_vector.end(), mh_addr.begin(), mh_addr.end());
    return addr_as_vector;
}

}