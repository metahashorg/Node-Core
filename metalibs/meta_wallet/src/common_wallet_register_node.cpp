#include <meta_constants.hpp>
#include <meta_wallet.h>

namespace metahash::meta_wallet {

bool CommonWallet::register_node(Wallet*, const transaction::TX* tx)
{
    uint64_t w_state = get_state();

    auto&& set_state_by_type = [&w_state](const std::string& type) {
        if (type == "Proxy") {
            w_state |= NODE_STATE_FLAG_PROXY_PRETEND;
        } else if (type == "InfrastructureTorrent") {
            w_state |= NODE_STATE_FLAG_INFRASTRUCTURETORRENT_PRETEND;
        } else if (type == "Torrent") {
            w_state |= NODE_STATE_FLAG_TORRENT_PRETEND;
        } else if (type == "Verifier") {
            w_state |= NODE_STATE_FLAG_VERIFIER_PRETEND;
        } else if (type == "Core") {
            w_state |= NODE_STATE_FLAG_CORE_PRETEND;
        } else if (type == "Auto") {
            w_state |= NODE_STATE_FLAG_PROXY_PRETEND;
            w_state |= NODE_STATE_FLAG_INFRASTRUCTURETORRENT_PRETEND;
            w_state |= NODE_STATE_FLAG_TORRENT_PRETEND;
            w_state |= NODE_STATE_FLAG_VERIFIER_PRETEND;
            w_state |= NODE_STATE_FLAG_CORE_PRETEND;
        }
    };

    w_state &= ~NODE_STATE_FLAG_PROXY_PRETEND;
    w_state &= ~NODE_STATE_FLAG_INFRASTRUCTURETORRENT_PRETEND;
    w_state &= ~NODE_STATE_FLAG_TORRENT_PRETEND;
    w_state &= ~NODE_STATE_FLAG_VERIFIER_PRETEND;
    w_state &= ~NODE_STATE_FLAG_CORE_PRETEND;

    if (tx->json_rpc->parameters.find("type") != tx->json_rpc->parameters.end()) {
        const auto& type = tx->json_rpc->parameters["type"];
        auto start = 0U;
        auto&& end = type.find('|');
        while (end != std::string::npos) {
            set_state_by_type(type.substr(start, end - start));
            start = end + 1;
            end = type.find('|', start);
        }

        set_state_by_type(type.substr(start, end));
    } else {
        w_state |= NODE_STATE_FLAG_PROXY_PRETEND;
    }

    set_state(w_state);

    return true;
}

}