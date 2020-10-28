#include "controller.hpp"

namespace metahash::meta_core {

bool ControllerImplementation::Blocks::contains(const sha256_2& hash)
{
    std::shared_lock lock(blocks_lock);
    return blocks.count(hash);
}

bool ControllerImplementation::Blocks::contains_next(const sha256_2& hash)
{
    std::shared_lock lock(blocks_lock);
    return previous.count(hash);
}

void ControllerImplementation::Blocks::insert(block::Block* block)
{
    sha256_2 hash = block->get_block_hash();
    sha256_2 prev = block->get_prev_hash();

    std::unique_lock lock(blocks_lock);
    if (blocks.count(hash) || previous.count(prev)) {
        delete block;
        return;
    }

    blocks.insert({ hash, block });
    previous.insert({ prev, block });
}

block::Block* ControllerImplementation::Blocks::operator[](const sha256_2& hash)
{
    std::shared_lock lock(blocks_lock);
    auto block_it = blocks.find(hash);

    if (block_it != blocks.end()) {
        return block_it->second;
    }

    return nullptr;
}

block::Block* ControllerImplementation::Blocks::get_next(const sha256_2& hash)
{
    std::shared_lock lock(blocks_lock);
    auto prev_it = previous.find(hash);

    if (prev_it != previous.end()) {
        return prev_it->second;
    }

    return nullptr;
}

void ControllerImplementation::Blocks::erase(const sha256_2& hash)
{
    std::unique_lock lock(blocks_lock);

    auto block_it = blocks.find(hash);

    if (block_it == blocks.end()) {
        return;
    }

    auto* block = block_it->second;

    auto range = previous.equal_range(block_it->second->get_prev_hash());

    for (auto prev_it = range.first; prev_it != range.second; ++prev_it) {
        if (prev_it->second == block) {
            previous.erase(prev_it);
            break;
        }
    }

    blocks.erase(block_it);
    delete block;
}

}