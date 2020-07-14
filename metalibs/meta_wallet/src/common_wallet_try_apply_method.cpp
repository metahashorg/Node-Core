#include <meta_log.hpp>
#include <meta_wallet.h>

namespace metahash::meta_wallet {

bool CommonWallet::try_apply_method(Wallet* other, transaction::TX const* tx)
{
    if (!tx->json_rpc) {
        //        DEBUG_COUT("no json present");
        return true;
    }

    const auto& method = tx->json_rpc->method;

    if (method == "delegate") {
        if (!try_delegate(other, tx)) {
            DEBUG_COUT("delegate failed");
            return false;
        }
    } else if (method == "undelegate") {
        if (!try_undelegate(other, tx)) {
            DEBUG_COUT("#undelegate failed");
            return false;
        }
    } else if (method == "mhRegisterNode") {
        if (!register_node(other, tx)) {
            DEBUG_COUT("#mhRegisterNode failed");
            return false;
        }
    } else if (method == "mh-noderegistration") {
        if (!register_node(other, tx)) {
            DEBUG_COUT("#mh-noderegistration failed");
            return false;
        }
        //    } else if (method == "dapp_create") {
        //        auto* wallet_to = dynamic_cast<DecentralizedApplication*>(other);
        //        if (!wallet_to) {
        //            DEBUG_COUT("invalid wallet type");
        //            return false;
        //        }
        //
        //        return wallet_to->try_dapp_create(tx);
        //    } else if (method == "dapp_modify") {
        //        auto* wallet_to = dynamic_cast<DecentralizedApplication*>(other);
        //        if (!wallet_to) {
        //            DEBUG_COUT("invalid wallet type");
        //            return false;
        //        }
        //
        //        return wallet_to->try_dapp_modify(tx);
        //    } else if (method == "dapp_add_host") {
        //        auto* wallet_to = dynamic_cast<DecentralizedApplication*>(other);
        //        if (!wallet_to) {
        //            DEBUG_COUT("invalid wallet type");
        //            return false;
        //        }
        //
        //        return wallet_to->try_dapp_add_host(tx);
        //    } else if (method == "dapp_del_host") {
        //        auto* wallet_to = dynamic_cast<DecentralizedApplication*>(other);
        //        if (!wallet_to) {
        //            DEBUG_COUT("invalid wallet type");
        //            return false;
        //        }
        //        return wallet_to->try_dapp_del_host(tx);
    }

    return true;
}

}