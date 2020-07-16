#include <meta_constants.hpp>
#include <meta_log.hpp>
#include <meta_wallet.h>

namespace metahash::meta_wallet {

CommonWallet::~CommonWallet()
{
    delete real_addition;
    delete addition;
}

CommonWallet::CommonWallet(std::deque<Wallet*>& _changed_wallets)
    : Wallet(_changed_wallets)
{
}

uint64_t CommonWallet::sub(Wallet* other, transaction::TX const* tx, uint64_t real_fee)
{
    uint64_t total_sub = tx->value + real_fee;

    if (get_balance() < total_sub) {
        DEBUG_COUT("insuficent funds");
        return TX_REJECT_INSUFFICIENT_FUNDS_EXT;
    }

    if (addition && addition->founder && addition->used_limit + total_sub > addition->limit) {
        DEBUG_COUT("founder limits");
        return TX_REJECT_FOUNDER_LIMIT;
    }

    uint64_t sub_result = Wallet::sub(other, tx, real_fee);

    if (addition && addition->founder && sub_result == 0) {
        addition->used_limit += total_sub;
    }

    return sub_result;
}

void CommonWallet::apply()
{
    delete real_addition;

    if (addition) {
        real_addition = new WalletAdditions(*addition);
    } else {
        real_addition = nullptr;
    }

    Wallet::apply();
}

void CommonWallet::clear()
{
    delete addition;

    if (real_addition) {
        addition = new WalletAdditions(*real_addition);
    } else {
        addition = nullptr;
    }

    Wallet::clear();
}

void CommonWallet::set_founder_limit()
{
    if (!addition) {
        addition = new WalletAdditions();
    }
    addition->founder = true;
    addition->limit += FOUNDER_INITIAL_LIMIT;
    changed_wallets.push_back(this);
}

}