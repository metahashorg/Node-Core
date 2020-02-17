#include "wallet.h"
#include <meta_log.hpp>
#include <open_ssl_decor.h>
#include <statics.hpp>

#include <set>

Wallet::Wallet(std::unordered_set<Wallet*>& _changed_wallets)
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

    changed_wallets.insert(this);
    return true;
}

void Wallet::add(uint64_t value)
{
    balance += value;
    changed_wallets.insert(this);
}

uint64_t Wallet::sub(Wallet* other, const TX* tx, uint64_t real_fee)
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

    changed_wallets.insert(this);
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

Wallet* WalletMap::get_wallet(const std::string& addres)
{
    if (wallet_map.find(addres) == wallet_map.end()) {
        wallet_map.insert({ addres, wallet_factory(addres) });
    }
    return wallet_map.at(addres);
}

Wallet* WalletMap::wallet_factory(const std::string& addr)
{
    auto bin_addr = hex2bin(addr);
    if (bin_addr.size() != 25) {
        return nullptr;
    }

    uint8_t int_family = bin_addr[0];

    if (addr == MASTER_WALLET_COIN_FORGING) {
        return new CommonWallet(changed_wallets);
    }
    if (addr == MASTER_WALLET_NODE_FORGING) {
        return new CommonWallet(changed_wallets);
    }
    if (addr == SPECIAL_WALLET_COMISSIONS) {
        return new CommonWallet(changed_wallets);
    }
    if (addr == STATE_FEE_WALLET) {
        return new CommonWallet(changed_wallets);
    }
    if (addr == ZERO_WALLET) {
        return new CommonWallet(changed_wallets);
    }

    switch (int_family) {
    case 0x00:
        return new CommonWallet(changed_wallets);
    case 0x16:
        return new DecentralizedApplication(changed_wallets);
    default:
        return new CommonWallet(changed_wallets);
    }
}

void WalletMap::apply_changes()
{
    for (auto wallet : changed_wallets) {
        wallet->apply();
    }
    changed_wallets.clear();
}

void WalletMap::clear_changes()
{
    for (auto wallet : changed_wallets) {
        wallet->clear();
    }
    changed_wallets.clear();
}
