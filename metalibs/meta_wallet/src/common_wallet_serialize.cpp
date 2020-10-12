#include <meta_wallet.h>

#include <rapidjson/writer.h>

namespace metahash::meta_wallet {

std::tuple<uint64_t, uint64_t, std::string> CommonWallet::serialize()
{
    std::string data;

    if (addition) {
        bool has_json = false;

        rapidjson::StringBuffer s;
        rapidjson::Writer<rapidjson::StringBuffer> writer(s);

        writer.StartObject();

        if (addition->founder) {
            has_json = true;
            writer.String("used_limit");
            writer.Uint64(addition->used_limit);

            writer.String("limit");
            writer.Uint64(addition->limit);
        }

        if (addition->state || addition->trust != 2) {
            has_json = true;
            writer.String("state");
            writer.Uint64(get_state());

            writer.String("trust");
            writer.Uint64(get_trust() * 5);
        }

        if (!addition->delegate_to_daly_snapshot.empty()) {
            has_json = true;
            writer.String("delegate_to");
            writer.StartArray();
            for (auto&& [a, v] : addition->delegate_to_daly_snapshot) {
                writer.StartObject();
                writer.String("a");
                writer.String(a.c_str());
                writer.String("v");
                writer.Uint64(v);
                writer.EndObject();
            }
            writer.EndArray();
        }

        if (!addition->delegated_from_daly_snapshot.empty()) {
            has_json = true;
            writer.String("delegated_from");
            writer.StartArray();
            for (auto&& [a, v] : addition->delegated_from_daly_snapshot) {
                writer.StartObject();
                writer.String("a");
                writer.String(a.c_str());
                writer.String("v");
                writer.Uint64(v);
                writer.EndObject();
            }
            writer.EndArray();
        }

        writer.EndObject();

        if (has_json) {
            data = std::string(s.GetString());
        }
    }

    return { balance, transaction_id, data };
}

}