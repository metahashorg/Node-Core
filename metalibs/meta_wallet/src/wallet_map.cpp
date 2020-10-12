#include <meta_constants.hpp>
#include <meta_wallet.h>

namespace metahash::meta_wallet {

Wallet* WalletMap::get_wallet(const std::string& address)
{
    if (wallet_map.find(address) == wallet_map.end()) {
        wallet_map.insert({ address, wallet_factory(address) });
    }
    return wallet_map.at(address);
}

Wallet* WalletMap::wallet_factory(const std::string& addr)
{
    auto bin_addr = crypto::hex2bin(addr);
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

}