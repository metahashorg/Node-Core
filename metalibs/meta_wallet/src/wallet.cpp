#include <meta_constants.hpp>
#include <meta_log.hpp>
#include <meta_wallet.h>

namespace metahash::meta_wallet {

Wallet::Wallet(std::deque<Wallet*>& _changed_wallets)
    : changed_wallets(_changed_wallets)
{
}

uint64_t Wallet::get_value()
{
    return balance;
}

bool Wallet::initialize(uint64_t value, uint64_t nonce, const std::string&)
{
    //    DEBUG_COUT("#");
    balance = value;
    transaction_id = nonce;

    changed_wallets.push_back(this);
    return true;
}

void Wallet::add(uint64_t value)
{
    balance += value;
    changed_wallets.push_back(this);
}

uint64_t Wallet::sub(Wallet* other, const transaction::TX* tx, uint64_t real_fee)
{
    uint64_t total_sub = tx->value + real_fee;

    if (tx->fee < real_fee) {
        DEBUG_COUT("insufficient fee");
        return TX_REJECT_INSUFFICIENT_FEE;
    }
    if (balance < total_sub) {
        DEBUG_COUT("insuficent funds");
        return TX_REJECT_INSUFFICIENT_FUNDS;
    }
    if (transaction_id != tx->nonce) {
        DEBUG_COUT("invalid nonce");
        return TX_REJECT_INVALID_NONCE;
    }

    balance -= total_sub;
    transaction_id++;

    other->add(tx->value);

    changed_wallets.push_back(this);
    return 0;
}

void Wallet::apply()
{
    real_transaction_id = transaction_id;
    real_balance = balance;
}

void Wallet::clear()
{
    transaction_id = real_transaction_id;
    balance = real_balance;
}

}