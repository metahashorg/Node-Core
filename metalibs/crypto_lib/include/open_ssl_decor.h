#pragma once

#ifndef CRYPTO_TEMPLATES_HPP
#define CRYPTO_TEMPLATES_HPP

// Containers
#include <array>
#include <vector>

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
    std::vector<char> private_key;
    std::vector<char> public_key;
    std::string mh_addr;

public:
    template <typename Container>
    Signer(const Container& private_file);

    template <typename Container>
    std::vector<char> sign(const Container& data);

    const std::vector<char>& get_pub_key();
    const std::string& get_mh_addr();

private:
    std::vector<char> sign(std::string_view data);
    void init(std::string_view data);
};

using sha256_2 = std::array<unsigned char, 32>;

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

std::string get_contract_address(const std::string& addr, uint64_t nonce);

template <typename PubKContainer, typename PrivKContainer>
bool generate_public_key(PubKContainer& pub_key, const PrivKContainer& private_key);

template <typename DataContainer, typename SignContainer, typename PrivKContainer>
bool sign_data(const DataContainer& data, SignContainer& sign, const PrivKContainer& private_key);

}

#include "open_ssl_decor_template_realization.hpp"

#endif // CRYPTO_TEMPLATES_HPP
