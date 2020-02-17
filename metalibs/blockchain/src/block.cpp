#include <block.h>
#include <meta_log.hpp>
#include <open_ssl_decor.h>
#include <statics.hpp>

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

uint64_t Block::get_block_type()
{
    return block_type;
}

uint64_t Block::get_block_timestamp()
{
    return block_timestamp;
}

sha256_2 Block::get_prev_hash()
{
    return prev_hash;
}

sha256_2 Block::get_block_hash()
{
    return block_hash;
}
