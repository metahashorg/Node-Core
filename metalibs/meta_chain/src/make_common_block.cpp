#include <meta_chain.h>

#include <meta_constants.hpp>
#include <meta_log.hpp>

namespace metahash::meta_chain {

block::Block* BlockChain::make_common_block(uint64_t timestamp, std::vector<transaction::TX*>& transactions)
{
    const uint64_t block_type = BLOCK_TYPE_COMMON;
    std::vector<char> txs_buff;
    auto state_fee = wallet_map.get_wallet(STATE_FEE_WALLET);

    if (!transactions.empty()) {
        uint64_t fee = 0;
        uint64_t tx_aprove_count = 0;
        {
            fee = get_fee(transactions.size());

            std::vector<char> fee_tx;

            auto bin_addres = crypto::hex2bin(SPECIAL_WALLET_COMISSIONS);

            fee_tx.insert(fee_tx.end(), bin_addres.begin(), bin_addres.end());

            crypto::append_varint(fee_tx, fee);
            crypto::append_varint(fee_tx, 0);
            crypto::append_varint(fee_tx, 0);

            crypto::append_varint(fee_tx, 0);
            crypto::append_varint(fee_tx, 0);
            crypto::append_varint(fee_tx, 0);

            crypto::append_varint(fee_tx, TX_STATE_FEE);

            crypto::append_varint(txs_buff, fee_tx.size());
            txs_buff.insert(txs_buff.end(), fee_tx.begin(), fee_tx.end());
        }

        std::sort(transactions.begin(), transactions.end(), [](transaction::TX* lh, transaction::TX* rh) {
            return lh->nonce < rh->nonce;
        });

        //check temp balances
        for (auto* tx : transactions) {
            const std::string& addr_from = tx->addr_from;
            const std::string& addr_to = tx->addr_to;

            uint64_t state = 0;

            if (test_nodes.find(addr_from) != test_nodes.end() && tx->json_rpc) {
                statistics_tx_list.push_back(tx);
                continue;
            }
            if (addr_from == ZERO_WALLET) {
                reject(tx, TX_REJECT_ZERO);
                delete tx;
                continue;
            }

            auto wallet_to = wallet_map.get_wallet(addr_to);
            auto wallet_from = wallet_map.get_wallet(addr_from);

            if (!wallet_to || !wallet_from) {
                reject(tx, TX_REJECT_INVALID_WALLET);
                delete tx;
                continue;
            }

            if (uint64_t status = wallet_from->sub(wallet_to, tx, fee + (tx->raw_tx.size() > 254 ? tx->raw_tx.size() - 254 : 0)) > 0) {
                reject(tx, status);
                delete tx;
                continue;
            }

            state_fee->add(fee + (tx->raw_tx.size() > 254 ? tx->raw_tx.size() - 254 : 0));

            if (!wallet_from->try_apply_method(wallet_to, tx)) {
                state = TX_STATE_WRONG_DATA;
            }

            if (state == 0) {
                state = TX_STATE_ACCEPT;
            }

            {
                std::vector<unsigned char> state_as_varint_array = crypto::int_as_varint_array(state);
                crypto::append_varint(txs_buff, tx->raw_tx.size() + state_as_varint_array.size());
                txs_buff.insert(txs_buff.end(), tx->raw_tx.begin(), tx->raw_tx.end());
                txs_buff.insert(txs_buff.end(), state_as_varint_array.begin(), state_as_varint_array.end());
            }

            tx_aprove_count++;
            delete tx;
        }

        txs_buff.push_back(0);

        wallet_map.clear_changes();
        transactions.resize(0);

        if (tx_aprove_count == 0) {
            return nullptr;
        }

        return make_block(block_type, timestamp, prev_hash, txs_buff);
    }
    return nullptr;
}

}