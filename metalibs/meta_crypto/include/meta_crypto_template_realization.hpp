#pragma once

#ifndef META_CRYPTO_HPP_TEMPLATE_REALIZATION_HPP
#define META_CRYPTO_HPP_TEMPLATE_REALIZATION_HPP

// Containers
#include <array>
#include <bitset>
#include <iomanip>
#include <sstream>
#include <vector>

// OpenSSL
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/ripemd.h>
#include <openssl/sha.h>
#include <openssl/x509v3.h>

#include <xxhash.h>

namespace metahash::crypto {

const uint8_t BYTED_2 = 0xfa;
const uint8_t BYTED_4 = 0xfb;
const uint8_t BYTED_8 = 0xfc;
const uint8_t BYTED_16 = 0xfd;
const uint8_t BYTED_32 = 0xfe;
const uint8_t BYTED_64 = 0xff;

using sha256_2 = std::array<unsigned char, 32>;

template <typename Container>
uint64_t Hasher::operator()(const Container& data) const
{
    return XXH64(data.data(), data.size(), 0);
}

template <typename Container>
Signer::Signer(const Container& private_file)
{
    init(std::string_view(reinterpret_cast<const char*>(private_file.data()), private_file.size()));
}

template <typename Container>
std::vector<char> Signer::sign(const Container& data)
{
    return sign_data(data, private_key);
}

std::string bin2hex(const unsigned char* data, uint64_t size);

template <typename Container>
std::string bin2hex(const Container& bin_msg)
{
    return bin2hex(reinterpret_cast<const unsigned char*>(bin_msg.data()), bin_msg.size());
}

template <typename T>
std::string int2hex(T i)
{
    std::stringstream stream;
    stream << "0x"
           << std::setfill('0') << std::setw(sizeof(T) * 2)
           << std::hex << i;
    return stream.str();
}

template <typename T>
std::string int2bin(T i)
{
    std::bitset<sizeof(T) * 8> x(i);
    std::stringstream stream;
    stream << "0b" << x;
    return stream.str();
}

uint8_t read_varint(uint64_t& varint, const char* data, uint64_t size);

template <typename Message>
uint8_t read_varint(uint64_t& varint, const Message& data)
{
    return read_varint(varint, data.data(), data.size());
}

template <typename Message>
uint64_t append_varint(Message& data, uint64_t value)
{
    std::string_view p_int(reinterpret_cast<char*>(&value), sizeof(value));

    uint64_t written = 0;

    std::vector<unsigned char> ret_data;
    if (value < 0xfa) {
        data.insert(data.end(), p_int.begin(), p_int.begin() + 1);
        written = 1;
    } else if (value <= 0xffff) {
        data.insert(data.end(), &BYTED_2, &BYTED_2 + 1);
        data.insert(data.end(), p_int.begin(), p_int.begin() + 2);
        written = 3;
    } else if (value <= 0xffffffff) {
        data.insert(data.end(), &BYTED_4, &BYTED_4 + 1);
        data.insert(data.end(), p_int.begin(), p_int.begin() + 4);
        written = 5;
    } else {
        data.insert(data.end(), &BYTED_8, &BYTED_8 + 1);
        data.insert(data.end(), p_int.begin(), p_int.begin() + 8);
        written = 9;
    }

    return written;
}

template <typename Message>
uint64_t get_xxhash64(const Message& data)
{
    return XXH64(data.data(), data.size(), 0);
}

template <typename Message>
sha256_2 get_sha256(const Message& data)
{
    sha256_2 hash1;
    sha256_2 hash2;

    const auto* data_buff = reinterpret_cast<const unsigned char*>(data.data());
    // First pass
    SHA256(data_buff, data.size(), hash1.data());

    // Second pass
    SHA256(hash1.data(), hash1.size(), hash2.data());

    return hash2;
}

bool CheckBufferSignature(EVP_PKEY* publicKey, const std::vector<char>& data, ECDSA_SIG* signature);

template <typename SignContainer>
ECDSA_SIG* ReadSignature(const SignContainer& binsign)
{
    const auto* data = reinterpret_cast<const unsigned char*>(binsign.data());

    ECDSA_SIG* signature = d2i_ECDSA_SIG(nullptr, &data, binsign.size());
    return signature;
}

template <typename PubKContainer>
EVP_PKEY* ReadPublicKey(const PubKContainer& binpubk)
{
    const auto* data = reinterpret_cast<const unsigned char*>(binpubk.data());

    EVP_PKEY* key = d2i_PUBKEY(nullptr, &data, binpubk.size());
    return key;
}

template <typename PrivKContainer>
EVP_PKEY* ReadPrivateKey(const PrivKContainer& binprivk)
{
    const auto* data = reinterpret_cast<const unsigned char*>(binprivk.data());

    EVP_PKEY* key = d2i_AutoPrivateKey(nullptr, &data, binprivk.size());
    return key;
}

template <typename DataContainer, typename SignContainer, typename PubKContainer>
bool check_sign(const DataContainer& data, const SignContainer& sign, const PubKContainer& pubk)
{
    EVP_PKEY* pubkey = ReadPublicKey(pubk);
    if (!pubkey) {
        return false;
    }
    ECDSA_SIG* signature = ReadSignature(sign);
    if (!signature) {
        EVP_PKEY_free(pubkey);
        return false;
    }

    std::vector<char> data_as_vector;
    data_as_vector.insert(data_as_vector.end(), data.begin(), data.end());

    if (CheckBufferSignature(pubkey, data_as_vector, signature)) {
        EVP_PKEY_free(pubkey);
        ECDSA_SIG_free(signature);
        return true;
    }

    EVP_PKEY_free(pubkey);
    ECDSA_SIG_free(signature);
    return false;
}

std::array<char, 25> make_address(std::vector<unsigned char>& bpubk);

template <typename PubKContainer>
std::array<char, 25> get_address(const PubKContainer& bpubk)
{
    std::vector<unsigned char> binary;
    binary.insert(binary.end(), bpubk.begin(), bpubk.end());

    return make_address(binary);
}

std::pair<unsigned char*, uint64_t> make_public_key(EVP_PKEY* pkey);

template <typename PubKContainer, typename PrivKContainer>
bool generate_public_key(PubKContainer& pub_key, const PrivKContainer& private_key)
{
    auto&& [public_key_data, public_key_size] = make_public_key(ReadPrivateKey(private_key));

    if (public_key_data) {
        pub_key.insert(pub_key.end(), public_key_data, public_key_data + public_key_size);
        free(public_key_data);

        return true;
    }
    return false;
}

std::vector<char> make_sign(const char* data, uint64_t size, EVP_PKEY* pkey);

template <typename DataContainer, typename PrivKContainer>
std::vector<char> sign_data(const DataContainer& data, const PrivKContainer& private_key)
{
    return make_sign(reinterpret_cast<const char*>(data.data()), data.size(), ReadPrivateKey(private_key));
}

}

#endif // META_CRYPTO_HPP_TEMPLATE_REALIZATION_HPP
