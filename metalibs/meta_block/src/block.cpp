#include <meta_block.h>
#include <meta_constants.hpp>
#include <meta_crypto.h>
#include <meta_log.hpp>

namespace metahash::block {

Block* parse_block(std::string_view block_sw)
{
    uint64_t block_type = 0;

    if (block_sw.size() < 8) {
        DEBUG_COUT("if (size < 8)");
        return nullptr;
    }

    block_type = *(reinterpret_cast<const uint64_t*>(&block_sw[0]));

    switch (block_type) {
    case BLOCK_TYPE:
    case BLOCK_TYPE_COMMON:
    case BLOCK_TYPE_STATE:
    case BLOCK_TYPE_FORGING: {
        auto block = new CommonBlock();
        if (block->parse(block_sw)) {
            return block;
        } else {
            delete block;
            return nullptr;
        }
    }
    case BLOCK_TYPE_TECH_BAD_TX: {
        auto block = new RejectedTXBlock();
        if (block->parse(block_sw)) {
            return block;
        } else {
            delete block;
            return nullptr;
        }
    }
    case BLOCK_TYPE_TECH_APPROVE: {
        auto block = new ApproveBlock();
        if (block->parse(block_sw)) {
            return block;
        } else {
            delete block;
            return nullptr;
        }
    }
    default:
        return nullptr;
    }
}

const std::vector<char>& Block::get_data()
{
    return data;
}

uint64_t Block::get_block_type() const
{
    return *(reinterpret_cast<const uint64_t*>(&data[0]));
}

uint64_t Block::get_block_timestamp() const
{
    return *(reinterpret_cast<const uint64_t*>(&data[8]));
}

sha256_2 Block::get_prev_hash() const
{
    sha256_2 prev_hash;
    std::copy_n(data.begin() + 16, 32, prev_hash.begin());
    return prev_hash;
}

sha256_2 Block::get_block_hash() const
{
    return crypto::get_sha256(data);
}

bool Block::is_local() const
{
    return from_local_storage;
}

void Block::set_local()
{
    from_local_storage = true;
}

}