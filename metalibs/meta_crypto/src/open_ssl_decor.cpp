#include "meta_crypto.h"

namespace metahash::crypto {

std::string bin2hex(const unsigned char* data, uint64_t size)
{
    static const char HexLookup[513] = {
        "000102030405060708090a0b0c0d0e0f"
        "101112131415161718191a1b1c1d1e1f"
        "202122232425262728292a2b2c2d2e2f"
        "303132333435363738393a3b3c3d3e3f"
        "404142434445464748494a4b4c4d4e4f"
        "505152535455565758595a5b5c5d5e5f"
        "606162636465666768696a6b6c6d6e6f"
        "707172737475767778797a7b7c7d7e7f"
        "808182838485868788898a8b8c8d8e8f"
        "909192939495969798999a9b9c9d9e9f"
        "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
        "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
        "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
        "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
        "e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
        "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"
    };

    std::string res;
    res.reserve(size * 2 + 1);

    for (uint i = 0; i < size; i++) {
        const char* hex = &HexLookup[2 * (static_cast<unsigned char>(data[i]))];
        res.insert(res.end(), hex, hex + 2);
    }

    return res;
}

uint8_t read_varint(uint64_t& varint, const char* data, uint64_t size)
{
    if (size < 1)
        return 0;

    auto data_0 = static_cast<uint8_t>(data[0]);
    if (data_0 < BYTED_2) {
        varint = data_0;
        return 1;
    }
    switch (data_0) {
    case BYTED_2: {
        if (size < 3)
            return 0;
        const auto* p_int16 = reinterpret_cast<const uint16_t*>(&data[1]);
        varint = *p_int16;
        return 3;
    }
    case BYTED_4: {
        if (size < 5)
            return 0;
        const auto* p_int32 = reinterpret_cast<const uint32_t*>(&data[1]);
        varint = *p_int32;
        return 5;
    }
    default: {
        if (size < 9)
            return 0;
        const auto* p_int64 = reinterpret_cast<const uint64_t*>(&data[1]);
        varint = *p_int64;
        return 9;
    }
    }
}

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

const std::vector<char>& Signer::get_pub_key()
{
    return public_key;
}

const std::string& Signer::get_mh_addr()
{
    return mh_addr;
}

void Signer::init(std::string_view priv_key_sw)
{
    const_cast<std::vector<char>*>(&private_key)->insert(const_cast<std::vector<char>*>(&private_key)->end(), priv_key_sw.begin(), priv_key_sw.end());
    if (!generate_public_key(*const_cast<std::vector<char>*>(&public_key), private_key)) {
        abort();
    }
    *const_cast<std::string*>(&mh_addr) = "0x" + bin2hex(get_address(public_key));
}

std::vector<std::string> split(const std::string& s, char delim)
{
    std::stringstream ss(s);
    std::string item;
    std::vector<std::string> elems;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

bool CheckBufferSignature(EVP_PKEY* publicKey, const std::vector<char>& data, ECDSA_SIG* signature)
{
    size_t bufsize = data.size();
    const char* buff = data.data();

    EVP_MD_CTX* mdctx;
    static const EVP_MD* md = EVP_sha256();
    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len;

    md = EVP_sha256();
    mdctx = EVP_MD_CTX_create();
    EVP_DigestInit_ex(mdctx, md, nullptr);
    EVP_DigestUpdate(mdctx, buff, bufsize);
    EVP_DigestFinal_ex(mdctx, md_value, &md_len);
    EVP_MD_CTX_destroy(mdctx);

    EC_KEY* ec_key = EVP_PKEY_get1_EC_KEY(publicKey);
    if (ECDSA_do_verify(md_value, md_len, signature, ec_key) == 1) {
        EC_KEY_free(ec_key);
        return true;
    }
    EC_KEY_free(ec_key);
    return false;
}

std::array<char, 25> make_address(std::vector<unsigned char>& bpubk)
{
    unsigned char* data = bpubk.data();
    int size = bpubk.size();
    if (data && size >= 65) {
        data[size - 65] = 0x04;

        sha256_2 sha_1;
        std::array<unsigned char, RIPEMD160_DIGEST_LENGTH> r160 {};

        SHA256(data + (size - 65), 65, sha_1.data());
        RIPEMD160(sha_1.data(), SHA256_DIGEST_LENGTH, r160.data());

        std::array<unsigned char, RIPEMD160_DIGEST_LENGTH + 1> wide_h {};
        wide_h[0] = 0;
        for (size_t i = 0; i < RIPEMD160_DIGEST_LENGTH; i++) {
            wide_h[i + 1] = r160[i];
        }

        sha256_2 hash1;
        SHA256(wide_h.data(), RIPEMD160_DIGEST_LENGTH + 1, hash1.data());

        sha256_2 hash2;
        SHA256(hash1.data(), SHA256_DIGEST_LENGTH, hash2.data());

        std::array<char, 25> address {};
        uint j = 0;
        {
            for (uint i = 0; i < wide_h.size(); i++, j++) {
                address[j] = static_cast<char>(wide_h[i]);
            }

            for (size_t i = 0; i < 4; i++, j++) {
                address[j] = static_cast<char>(hash2[i]);
            }
        }

        return address;
    }

    return std::array<char, 25> { 0 };
}

std::pair<unsigned char*, uint64_t> make_public_key(EVP_PKEY* pkey)
{
    if (!pkey) {
        return { nullptr, 0 };
    }

    unsigned char* public_key_temp_buff = nullptr;
    int public_key_temp_buff_size = i2d_PUBKEY(pkey, &public_key_temp_buff);

    EVP_PKEY_free(pkey);

    if (public_key_temp_buff_size <= 0) {
        return { nullptr, 0 };
    }

    return { public_key_temp_buff, public_key_temp_buff_size };
}

std::vector<char> make_sign(const char* data, uint64_t size, EVP_PKEY* pkey)
{
    if (!pkey) {
        return std::vector<char>();
    }

    EVP_MD_CTX* md = EVP_MD_CTX_create();
    if (!md) {
        EVP_PKEY_free(pkey);
        return std::vector<char>();
    }

    if (EVP_DigestSignInit(md, nullptr, EVP_sha256(), nullptr, pkey) != 1) {
        EVP_PKEY_free(pkey);
        EVP_MD_CTX_destroy(md);
        return std::vector<char>();
    }

    if (EVP_DigestSignUpdate(md, data, size) != 1) {
        EVP_PKEY_free(pkey);
        EVP_MD_CTX_destroy(md);
        return std::vector<char>();
    }

    size_t signature_size = 0;
    if (EVP_DigestSignFinal(md, nullptr, &signature_size) != 1) {
        EVP_PKEY_free(pkey);
        EVP_MD_CTX_destroy(md);
        return std::vector<char>();
    }

    std::vector<char> signature_temp_buff(signature_size);

    if (EVP_DigestSignFinal(md, reinterpret_cast<unsigned char*>(signature_temp_buff.data()), &signature_size) != 1) {
        EVP_PKEY_free(pkey);
        EVP_MD_CTX_destroy(md);
        return std::vector<char>();
    }

    EVP_PKEY_free(pkey);
    EVP_MD_CTX_destroy(md);

    return signature_temp_buff;
}

}