#include <set>

#include <meta_chain.h>

#include <meta_constants.hpp>
#include <meta_crypto.h>
#include <meta_log.hpp>

namespace metahash::meta_chain {

BlockChain::BlockChain(boost::asio::io_context& io_context)
    : io_context(io_context)
{
}

uint64_t BlockChain::get_fee(uint64_t cnt)
{
    static const uint64_t MAX_TRANSACTION_COUNT_20_of_100 = (MAX_TRANSACTION_COUNT * 20 / 100);
    if (cnt <= MAX_TRANSACTION_COUNT_20_of_100) {
        return COMISSION_COMMON_00_20;
    }
    if (cnt <= MAX_TRANSACTION_COUNT_20_of_100 * 2) {
        return COMISSION_COMMON_21_40;
    }
    if (cnt <= MAX_TRANSACTION_COUNT_20_of_100 * 3) {
        return COMISSION_COMMON_41_60;
    }
    if (cnt <= MAX_TRANSACTION_COUNT_20_of_100 * 4) {
        return COMISSION_COMMON_61_80;
    }
    return COMISSION_COMMON_81_99;
}

block::Block* BlockChain::make_block(uint64_t b_type, uint64_t b_time, sha256_2 prev_b_hash, std::vector<char>& tx_buff)
{
    std::vector<char> block_buff;
    sha256_2 tx_hash = crypto::get_sha256(tx_buff);

    block_buff.insert(block_buff.end(), reinterpret_cast<char*>(&b_type), (reinterpret_cast<char*>(&b_type) + sizeof(uint64_t)));
    block_buff.insert(block_buff.end(), reinterpret_cast<char*>(&b_time), (reinterpret_cast<char*>(&b_time) + sizeof(uint64_t)));
    block_buff.insert(block_buff.end(), prev_b_hash.begin(), prev_b_hash.end());
    block_buff.insert(block_buff.end(), tx_hash.begin(), tx_hash.end());
    block_buff.insert(block_buff.end(), tx_buff.begin(), tx_buff.end());

    std::string_view block_as_sw(block_buff.data(), block_buff.size());
    block::Block* block = block::parse_block(block_as_sw);
    if (block) {
        DEBUG_COUT("BLOCK IS OK");
        return block;
    }
    return nullptr;
}

void BlockChain::reject(const transaction::TX* tx, uint64_t reason)
{
    auto rejected_tx = new transaction::RejectedTXInfo();
    rejected_tx->make(tx->hash, reason);
    rejected_tx_list.push_back(rejected_tx);
}

void BlockChain::fill_node_state()
{
    std::unordered_map<std::string, std::set<std::string>, crypto::Hasher> states;

    for (auto&& [addr, p_wallet] : wallet_map) {
        auto* wallet = dynamic_cast<meta_wallet::CommonWallet*>(p_wallet);
        if (!wallet) {
            DEBUG_COUT("invalid wallet type");
            DEBUG_COUT(addr);
            continue;
        }

        uint64_t w_state = wallet->get_state();

        for (auto&& [role, mask] : NODE_STATE_FLAG_FORGING) {
            if ((w_state & mask) == mask) {
                states[addr].insert(role);
            }
        }
    }

    node_state.swap(states);
}

}