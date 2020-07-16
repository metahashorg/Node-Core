#include <meta_transaction.h>

namespace metahash::transaction {

TX::TX() = default;

TX::TX(const TX& other)
    : state(other.state)
    , tx_size(other.tx_size)
    , value(other.value)
    , fee(other.fee)
    , nonce(other.nonce)
{

    bin_to = other.bin_to;
    data = other.data;
    sign = other.sign;
    pub_key = other.pub_key;

    data_for_sign = other.data_for_sign;

    raw_tx = other.raw_tx;

    hash = other.hash;

    addr_from = other.addr_from;
    addr_to = other.addr_to;

    if (other.json_rpc) {
        json_rpc = new JSON_RPC;
        *json_rpc = *(other.json_rpc);
    }
}

TX::TX(TX&& other)
    : state(other.state)
    , tx_size(other.tx_size)
    , value(other.value)
    , fee(other.fee)
    , nonce(other.nonce)
{
    bin_to = std::move(other.bin_to);
    data = std::move(other.data);
    sign = std::move(other.sign);
    pub_key = std::move(other.pub_key);

    data_for_sign = std::move(other.data_for_sign);

    raw_tx = std::move(other.raw_tx);

    hash = std::move(other.hash);

    addr_from = std::move(other.addr_from);
    addr_to = std::move(other.addr_to);

    json_rpc = other.json_rpc;
    other.json_rpc = nullptr;
}

TX& TX::operator=(const TX& other)
{
    if (this != &other) {
        state = other.state;
        tx_size = other.tx_size;
        value = other.value;
        fee = other.fee;
        nonce = other.nonce;

        bin_to = other.bin_to;
        data = other.data;
        sign = other.sign;
        pub_key = other.pub_key;

        data_for_sign = other.data_for_sign;

        raw_tx = other.raw_tx;

        hash = other.hash;

        addr_from = other.addr_from;
        addr_to = other.addr_to;

        if (other.json_rpc) {
            json_rpc = new JSON_RPC;
            *json_rpc = *(other.json_rpc);
        }
    }

    return *this;
}

TX& TX::operator=(TX&& other)
{
    if (this != &other) {
        state = other.state;
        tx_size = other.tx_size;
        value = other.value;
        fee = other.fee;
        nonce = other.nonce;

        bin_to = std::move(other.bin_to);
        data = std::move(other.data);
        sign = std::move(other.sign);
        pub_key = std::move(other.pub_key);

        data_for_sign = std::move(other.data_for_sign);

        raw_tx = std::move(other.raw_tx);

        hash = std::move(other.hash);

        addr_from = std::move(other.addr_from);
        addr_to = std::move(other.addr_to);

        json_rpc = other.json_rpc;
        other.json_rpc = nullptr;
    }

    return *this;
}

TX::~TX()
{
    delete json_rpc;
}

}