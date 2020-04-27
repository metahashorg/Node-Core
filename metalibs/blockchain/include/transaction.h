#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <array>
#include <deque>
#include <map>
#include <string_view>
#include <vector>

using sha256_2 = std::array<unsigned char, 32>;

struct TX {
public:
    struct JSON_RPC {
        std::map<std::string, std::string> parameters;
        std::string method;
    };

    uint64_t state = 0;
    uint64_t tx_size = 0;

    std::string bin_to;
    uint64_t value;
    uint64_t fee;
    uint64_t nonce;
    std::string data;
    std::string sign;
    std::string pub_key;

    std::string data_for_sign;

    std::vector<char> raw_tx;

    sha256_2 hash;

    std::string addr_from;
    std::string addr_to;

    JSON_RPC* json_rpc = nullptr;

    TX();

    TX(const TX&);
    TX(TX&&);

    TX& operator=(const TX& other);
    TX& operator=(TX&& other);

    ~TX();

    bool parse(std::string_view raw_data, bool check_sign_flag = true);

    bool fill_from_strings(
        std::string& param_to,
        std::string param_value,
        std::string param_fee,
        std::string param_nonce,
        std::string& param_data,
        std::string& param_sign,
        std::string& param_pub_key);

    template <typename AddresContainer, typename DataContainer, typename SignContainer, typename PubKContainer>
    bool fill_sign_n_raw(
        AddresContainer& param_to,
        uint64_t param_value,
        uint64_t param_fee,
        uint64_t param_nonce,
        DataContainer& param_data,
        SignContainer& param_sign,
        PubKContainer& param_pub_key)
    {
        if (param_to.size() != 25)
            return false;
        std::vector<char> _raw_tx;

        //        uint64_t bin_to_index = _raw_tx.size();
        _raw_tx.insert(_raw_tx.end(), param_to.begin(), param_to.end());

        append_tx_varint(_raw_tx, param_value);
        append_tx_varint(_raw_tx, param_fee);
        append_tx_varint(_raw_tx, param_nonce);

        append_tx_varint(_raw_tx, param_data.size());
        _raw_tx.insert(_raw_tx.end(), param_data.begin(), param_data.end());

        append_tx_varint(_raw_tx, param_sign.size());
        _raw_tx.insert(_raw_tx.end(), param_sign.begin(), param_sign.end());

        append_tx_varint(_raw_tx, param_pub_key.size());
        _raw_tx.insert(_raw_tx.end(), param_pub_key.begin(), param_pub_key.end());

        std::string_view tx_sw(_raw_tx.data(), _raw_tx.size());

        return parse(tx_sw);
    }

private:
    void clear();
    void append_tx_varint(std::vector<char>& _raw_tx, uint64_t);
    bool check_tx();
};

struct ApproveRecord {
    std::vector<char> data;
    std::string_view block_hash;
    std::string_view sign;
    std::string_view pub_key;
    bool approve;

    bool parse(std::string_view);
    bool make(const sha256_2& block_hash, const std::vector<char>& PrivKey, const std::vector<char>& PubKey);
};

struct RejectedTXInfo {
    std::vector<char> data;
    sha256_2 tx_hash;
    uint64_t reason;

    bool parse(std::string_view);
    bool make(const sha256_2& tx_hash, uint64_t reason);
};

#endif // CRYPTO_TEMPLATES_HPP
