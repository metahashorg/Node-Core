#pragma once

#ifndef META_CRYPTO_HPP
#define META_CRYPTO_HPP

// Containers
#include <array>
#include <vector>
#include <string>
// OpenSSL
#include <openssl/ecdsa.h>
#include <openssl/evp.h>

namespace metahash::crypto {

struct Hasher {
    template <typename Container>
    uint64_t operator()(const Container& data) const;
};

class Signer {
private:
    const std::vector<char> private_key;
    const std::vector<char> public_key;
    const std::string mh_addr;

public:
    template <typename Container>
    Signer(const Container& private_file);

    template <typename Container>
    std::vector<char> sign(const Container& data);

    const std::vector<char>& get_pub_key();
    const std::string& get_mh_addr();

private:
    void init(std::string_view data);
};

using sha256_2 = std::array<unsigned char, 32>;

std::vector<std::string> split(const std::string& s, char delim);

std::vector<unsigned char> hex2bin(const std::string_view src);

template <typename Container>
std::string bin2hex(const Container& bin_msg);

template <typename T>
std::string int2hex(T i);

template <typename T>
std::string int2bin(T i);

template <typename Message>
uint8_t read_varint(uint64_t& varint, const Message& data);

std::vector<unsigned char> int_as_varint_array(uint64_t value);

template <typename Message>
uint64_t append_varint(Message& data, uint64_t value);

template <typename Message>
uint64_t get_xxhash64(const Message& data);

template <typename Message>
sha256_2 get_sha256(const Message& data);
template <typename DataContainer>
bool CheckBufferSignature(EVP_PKEY* publicKey, const DataContainer& data, ECDSA_SIG* signature);

template <typename SignContainer>
ECDSA_SIG* ReadSignature(const SignContainer& binsign);

template <typename PubKContainer>
EVP_PKEY* ReadPublicKey(const PubKContainer& binpubk);

template <typename PrivKContainer>
EVP_PKEY* ReadPrivateKey(const PrivKContainer& binprivk);

template <typename DataContainer, typename SignContainer, typename PubKContainer>
bool check_sign(const DataContainer& data, const SignContainer& sign, const PubKContainer& pubk);

template <typename PubKContainer>
std::array<char, 25> get_address(const PubKContainer& bpubk);

template <typename PubKContainer, typename PrivKContainer>
bool generate_public_key(PubKContainer& pub_key, const PrivKContainer& private_key);

template <typename DataContainer, typename PrivKContainer>
std::vector<char> sign_data(const DataContainer& data, const PrivKContainer& private_key);

}

#include "meta_crypto_template_realization.hpp"

#endif // META_CRYPTO_HPP
