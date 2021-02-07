#include <meta_client.h>
#include <meta_common.h>
//#include <meta_log.hpp>

namespace metahash::network {

int8_t ClientConnection::Response::parse(char* buff_data, size_t buff_size, const std::string& mh_endpoint_addr)
{
    request_full.insert(request_full.end(), buff_data, buff_data + buff_size);

    if (magic_number == 0) {
        if (request_full.size() >= sizeof(uint32_t)) {
            magic_number = *(reinterpret_cast<uint32_t*>(&request_full[0]));
            if (magic_number != METAHASH_MAGIC_NUMBER) {
                return statics::WRONG_MAGIC_NUMBER;
            }
            offset = sizeof(uint32_t);
        } else {
            return statics::INCOMPLETE;
        }
    }

    if (request_id == 0 && !read_varint(request_id)) {
        return statics::INCOMPLETE;
    }

    if (public_key_size == 0 && !read_varint(public_key_size)) {
        return statics::INCOMPLETE;
    }

    if (public_key.empty()) {
        if (fill_sw(public_key, public_key_size)) {
            sender_addr = "0x" + crypto::bin2hex(crypto::get_address(public_key));
            if (mh_endpoint_addr != sender_addr) {
                //DEBUG_COUT("UNKNOWN_SENDER_METAHASH_ADDRESS");
                return statics::UNKNOWN_SENDER_METAHASH_ADDRESS;
            }
        } else {
            return statics::INCOMPLETE;
        }
    }

    if (sign_size == 0 && !read_varint(sign_size)) {
        return statics::INCOMPLETE;
    }

    if (sign.empty() && !fill_sw(sign, sign_size)) {
        return statics::INCOMPLETE;
    }

    if (message_size == 0 && !read_varint(message_size)) {
        return statics::INCOMPLETE;
    }

    if (message_size && message.empty()) {
        if (fill_sw(message, message_size)) {
            if (!crypto::check_sign(message, sign, public_key)) {
                //DEBUG_COUT(crypto::bin2hex(sign));
                //DEBUG_COUT(crypto::bin2hex(public_key));
                //DEBUG_COUT("INVALID_SIGN");
                return statics::INVALID_SIGN;
            }
        } else {
            return statics::INCOMPLETE;
        }
    }

    return statics::SUCCESS;
}

bool ClientConnection::Response::read_varint(uint64_t& varint)
{
    auto previous_offset = offset;
    offset += crypto::read_varint(varint, std::string_view(&request_full[offset], request_full.size() - offset));
    return offset != previous_offset;
}

bool ClientConnection::Response::fill_sw(std::vector<char>& sw, uint64_t sw_size)
{
    if (offset + sw_size > request_full.size()) {
        return false;
    } else {
        sw.clear();
        sw.insert(sw.end(), &request_full[offset], &request_full[offset] + sw_size);
        offset += sw_size;
        return true;
    }
}

}
