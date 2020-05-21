#ifndef BLOCK_H
#define BLOCK_H

#include <open_ssl_decor.h>
#include <thread_pool.hpp>
#include <transaction.h>

namespace metahash::metachain {

class Block {
protected:
    std::vector<char> data;

public:
    virtual ~Block() = default;

    virtual const std::vector<char>& get_data();
    virtual uint64_t get_block_type();
    virtual uint64_t get_block_timestamp();
    virtual sha256_2 get_prev_hash();
    virtual sha256_2 get_block_hash();

    virtual bool parse(std::string_view block_sw) = 0;
};

class CommonBlock : public Block {
private:
    static const uint8_t tx_buff = 80;

public:
    virtual ~CommonBlock() override = default;

    uint64_t get_block_type() override;
    const std::vector<TX> get_txs();
    const std::vector<TX> get_txs(boost::asio::io_context& io_context);

    bool parse(std::string_view block_sw) override;
};

class ApproveBlock : public Block {
private:
    static const uint8_t tx_buff = 48;

public:
    virtual ~ApproveBlock() override = default;

    const std::vector<ApproveRecord> get_txs();

    bool parse(std::string_view block_sw) override;
    bool make(uint64_t, const sha256_2&, const std::vector<ApproveRecord*>&);
};

class RejectedTXBlock : public Block {
private:
    std::string_view sign;
    std::string_view pub_key;
    std::string_view data_for_sign;
    uint64_t tx_buff = 0;

public:
    virtual ~RejectedTXBlock() override = default;

    std::string get_sign();
    std::string get_pub_key();
    std::string get_data_for_sign();

    const std::vector<RejectedTXInfo> get_txs();

    bool parse(std::string_view block_sw) override;
    bool make(uint64_t timestamp, const sha256_2& new_prev_hash, const std::vector<RejectedTXInfo*>& new_txs, crypto::Signer& signer);
};

Block* parse_block(std::string_view);

}

#endif // BLOCK_H
