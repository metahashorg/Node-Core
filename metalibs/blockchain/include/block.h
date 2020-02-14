#ifndef BLOCK_H
#define BLOCK_H

#include <transaction.h>

class Block {
protected:
    std::vector<char> data;

    uint64_t block_type = 0;
    uint64_t block_timestamp = 0;

    sha256_2 prev_hash = { { 0 } };
    sha256_2 block_hash = { { 0 } };

public:
    Block() = default;
    Block(const Block&) = delete;
    Block(Block&&) = delete;
    Block& operator=(const Block& other) = delete;
    Block& operator=(Block&& other) = delete;

    virtual ~Block() = default;

    virtual const std::vector<char>& get_data();
    virtual uint64_t get_block_type();
    virtual uint64_t get_block_timestamp();
    virtual sha256_2 get_prev_hash();
    virtual sha256_2 get_block_hash();

    virtual bool parse(std::string_view block_sw) = 0;
    virtual void clean() = 0;
};

class CommonBlock : public Block {
private:
    sha256_2 tx_hash = { { 0 } };
    std::vector<TX*> txs;

public:
    CommonBlock() = default;
    CommonBlock(const CommonBlock&) = delete;
    CommonBlock(CommonBlock&&) = delete;
    CommonBlock& operator=(const CommonBlock& other) = delete;
    CommonBlock& operator=(CommonBlock&& other) = delete;
    ~CommonBlock() override;

    sha256_2 get_prev_hash() override;
    const std::vector<TX*>& get_txs();

    bool parse(std::string_view block_sw) override;
    void clean() override;
};

class ApproveBlock : public Block {
private:
    std::vector<ApproveRecord*> txs;

public:
    ApproveBlock() = default;
    ApproveBlock(const ApproveBlock&) = delete;
    ApproveBlock(ApproveBlock&&) = delete;
    ApproveBlock& operator=(const ApproveBlock& other) = delete;
    ApproveBlock& operator=(ApproveBlock&& other) = delete;
    ~ApproveBlock() override;

    const std::vector<ApproveRecord*>& get_txs();

    bool parse(std::string_view block_sw) override;
    bool make(uint64_t, const sha256_2&, const std::vector<ApproveRecord*>&);
    void clean() override;
};

class RejectedTXBlock : public Block {
private:
    std::string_view sign;
    std::string_view pub_key;
    std::string_view data_for_sign;
    std::vector<RejectedTXInfo*> txs;

public:
    RejectedTXBlock() = default;
    RejectedTXBlock(const RejectedTXBlock&) = delete;
    RejectedTXBlock(RejectedTXBlock&&) = delete;
    RejectedTXBlock& operator=(const RejectedTXBlock& other) = delete;
    RejectedTXBlock& operator=(RejectedTXBlock&& other) = delete;
    ~RejectedTXBlock() override;

    std::string get_sign();
    std::string get_pub_key();
    std::string get_data_for_sign();

    const std::vector<RejectedTXInfo*>& get_txs();

    bool parse(std::string_view block_sw) override;
    bool make(uint64_t timestamp, const sha256_2& new_prev_hash, const std::vector<RejectedTXInfo*>& new_txs, const std::vector<char>& PrivKey, const std::vector<char>& PubKey);

    void clean() override;
};

Block* parse_block(std::string_view);

#endif // BLOCK_H
