#include <meta_common.h>
#include <meta_server.h>

namespace metahash::network {

bool Request::read_varint(uint64_t& varint)
{
    auto previous_offset = offset;
    offset += crypto::read_varint(varint, std::string_view(&request_full[offset], request_full.size() - offset));
    return offset != previous_offset;
}

bool Request::fill_sw(std::string_view& sw, uint64_t sw_size)
{
    if (offset + sw_size > request_full.size()) {
        return false;
    } else {
        sw = std::string_view(&request_full[offset], sw_size);
        offset += sw_size;
        return true;
    }
}

int8_t Request::parse(char* buff_data, size_t buff_size, std::unordered_set<std::string, crypto::Hasher>& allowed_addreses)
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

    if (request_type == 0 && !read_varint(request_type)) {
        return statics::INCOMPLETE;
    }

    if (public_key_size == 0 && !read_varint(public_key_size)) {
        return statics::INCOMPLETE;
    }

    if (public_key.empty()) {
        if (fill_sw(public_key, public_key_size)) {
            sender_mh_addr = "0x" + crypto::bin2hex(crypto::get_address(public_key));
            if (!allowed_addreses.empty()) {
                if (allowed_addreses.find(sender_mh_addr) == allowed_addreses.end()) {
                    return statics::UNKNOWN_SENDER_METAHASH_ADDRESS;
                }
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
                return statics::INVALID_SIGN;
            }
        } else {
            return statics::INCOMPLETE;
        }
    }

    return statics::SUCCESS;
}

}