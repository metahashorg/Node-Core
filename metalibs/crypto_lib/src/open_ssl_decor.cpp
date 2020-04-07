#include "open_ssl_decor.h"

#include "keccak.h"

std::vector<unsigned char> int_as_varint_array(uint64_t value)
{
    auto* p_int = reinterpret_cast<unsigned char*>(&value);

    std::vector<unsigned char> ret_data;
    if (value < 0xfa) {
        ret_data.push_back(p_int[0]);
    } else if (value <= 0xffff) {
        ret_data.push_back(BYTED_2);
        ret_data.insert(ret_data.end(), p_int, p_int + 2);
    } else if (value <= 0xffffffff) {
        ret_data.push_back(BYTED_4);
        ret_data.insert(ret_data.end(), p_int, p_int + 4);
    } else {
        ret_data.push_back(BYTED_8);
        ret_data.insert(ret_data.end(), p_int, p_int + 8);
    }
    return ret_data;
}

std::vector<unsigned char> hex2bin(const std::string_view src)
{
    static const std::array<unsigned char, 256> DecLookup = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // gap before first hex digit
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, // 0123456789
        0, 0, 0, 0, 0, 0, 0, // :;<=>?@ (gap)
        10, 11, 12, 13, 14, 15, // ABCDEF
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // GHIJKLMNOPQRS (gap)
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // TUVWXYZ[/]^_` (gap)
        10, 11, 12, 13, 14, 15, // abcdef
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 // fill zeroes
    };

    uint i = 0;
    if (src.size() > 2 && src[0] == '0' && src[1] == 'x') {
        i = 2;
    }

    std::vector<unsigned char> dest;
    dest.reserve(src.length() / 2);
    for (; i < src.length(); i += 2) {
        unsigned char d = DecLookup.at((unsigned char)src[i]) << 4;
        d |= DecLookup.at((unsigned char)src[i + 1]);
        dest.push_back(d);
    }

    return dest;
}

template <typename Message>
sha256_2 keccak(const Message& data)
{
    sha256_2 hs;
    CryptoPP::Keccak k(SHA256_DIGEST_LENGTH);
    k.Update(data.data(), data.size());
    k.TruncatedFinal(hs.data(), SHA256_DIGEST_LENGTH);
    return hs;
}

std::vector<unsigned char> IntToRLP(uint64_t value)
{
    std::vector<unsigned char> ret_data;

    if (value == 0) {
        ret_data.push_back(0);
        return ret_data;
    }

    std::string_view p_int(reinterpret_cast<char*>(&value), sizeof(value));

    uint8_t i = 0;
    while (*(p_int.rbegin() + i) == 0) {
        i++;
    }
    ret_data.insert(ret_data.end(), p_int.rbegin() + i, p_int.rend());

    return ret_data;
}

std::vector<unsigned char> EncodeField(const std::vector<unsigned char>& field)
{
    std::vector<unsigned char> rslt;

    uint64_t fs = field.size();
    if (fs == 1 && (uint8_t)field.at(0) <= 0x7F) {
        if (field.at(0) == 0) {
            rslt.push_back(0x80);
        } else {
            rslt.insert(rslt.end(), field.begin(), field.end());
        }
    } else if (fs <= 55) {
        rslt.push_back(0x80 + fs);
        rslt.insert(rslt.end(), field.begin(), field.end());
    } else /*if (fs > 55)*/ {
        std::vector<unsigned char> bigint = IntToRLP(fs);

        rslt.push_back(0xB7 + bigint.size());
        rslt.insert(rslt.end(), bigint.begin(), bigint.end());
        rslt.insert(rslt.end(), field.begin(), field.end());
    }

    return rslt;
}

std::vector<unsigned char> CalcTotalSize(const std::vector<unsigned char>& dump)
{
    std::vector<unsigned char> rslt;
    uint64_t ds = dump.size();
    if (ds <= 55) {
        rslt.push_back(0xC0 + ds);
        rslt.insert(rslt.end(), dump.begin(), dump.end());
    } else {
        std::vector<unsigned char> bigint = IntToRLP(ds);

        rslt.push_back(0xF7 + bigint.size());
        rslt.insert(rslt.end(), bigint.begin(), bigint.end());
        rslt.insert(rslt.end(), dump.begin(), dump.end());
    }

    return rslt;
}

std::vector<unsigned char> RLP(const std::vector<std::vector<unsigned char>>& fields)
{
    std::vector<unsigned char> dump;
    for (const auto& field : fields) {
        std::vector<unsigned char> field_encoded = EncodeField(field);
        dump.insert(dump.end(), field_encoded.begin(), field_encoded.end());
    }
    dump = CalcTotalSize(dump);
    return dump;
}

std::string get_contract_address(const std::string& addr, uint64_t nonce)
{
    auto bin_addr = hex2bin(addr);

    std::vector<std::vector<unsigned char>> fields;
    fields.push_back(bin_addr);
    std::vector<unsigned char> rlpnonce = IntToRLP(nonce);
    fields.push_back(rlpnonce);
    std::vector<unsigned char> rlpenc = RLP(fields);
    sha256_2 keccak_hash = keccak(rlpenc);
    std::vector<unsigned char> address;
    address.push_back(0x08);
    address.insert(address.end(), keccak_hash.begin() + 12, keccak_hash.end());
    sha256_2 sha2_hash = get_sha256(address);
    address.insert(address.end(), sha2_hash.begin(), sha2_hash.begin() + 4);
    return "0x" + bin2hex(address);
}
