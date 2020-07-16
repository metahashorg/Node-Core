#include <meta_wallet.h>

namespace metahash::meta_wallet {

uint64_t CommonWallet::get_state()
{
    if (addition) {
        return addition->state;
    }
    return 0;
}

void CommonWallet::set_state(uint64_t new_state)
{
    if (!addition) {
        addition = new WalletAdditions();
    }
    addition->state = new_state;
    changed_wallets.push_back(this);
}

uint64_t CommonWallet::get_trust()
{
    if (addition) {
        return addition->trust;
    }

    return 2;
}

void CommonWallet::add_trust()
{
    if (!addition) {
        addition = new WalletAdditions();
    }
    if (addition->trust < 200) {
        addition->trust++;
    } else {
        addition->trust = 200;
    }
    changed_wallets.push_back(this);
}

void CommonWallet::sub_trust()
{
    if (!addition) {
        addition = new WalletAdditions();
    }
    if (addition->trust >= 12) {
        addition->trust -= 10;
    } else {
        addition->trust = 2;
    }
    changed_wallets.push_back(this);
}

void CommonWallet::set_trust(uint64_t new_trust)
{
    if (!addition) {
        addition = new WalletAdditions();
    }
    addition->trust = new_trust;

    changed_wallets.push_back(this);
}

}