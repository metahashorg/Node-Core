#include "core_service.h"

#include <meta_crypto.h>
#include <meta_log.hpp>
#include <rapidjson/document.h>

#include <filesystem>
#include <fstream>

void parse_settings(
    const std::string& file_name,
    std::string& network,
    std::string& tx_host,
    int& tx_port,
    std::string& path,
    std::string& hash,
    std::string& key,
    std::map<std::string, std::pair<std::string, int>>& core_list)
{
    std::ifstream ifs(file_name);
    std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));

    rapidjson::Document config_json;
    if (!config_json.Parse(content.c_str()).HasParseError()) {

        if (config_json.HasMember("network") && config_json["network"].IsString()) {
            network = config_json["network"].GetString();
            if (network != "net-main" && network != "net-dev" && network != "net-test") {
                DEBUG_COUT("Unknown network name:\t" + network);
                print_config_file_params_and_exit();
            }

            DEBUG_COUT("Network name:\t" + network);
        } else {
            DEBUG_COUT("Missing network name");
            print_config_file_params_and_exit();
        }

        if (config_json.HasMember("hostname") && config_json["hostname"].IsString() && config_json.HasMember("port") && config_json["port"].IsUint() && config_json["port"].GetInt() != 0) {
            tx_host = config_json["hostname"].GetString();
            tx_port = config_json["port"].GetInt();
        } else {
            DEBUG_COUT("Invalid or missing listening host or port");
            print_config_file_params_and_exit();
        }

        if (config_json.HasMember("path") && config_json["path"].IsString()) {
            path = config_json["path"].GetString();

            if (std::filesystem::path dir(path); !std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
                DEBUG_COUT("Invalid metachain path:\t" + path);
                print_config_file_params_and_exit();
            }

            DEBUG_COUT("Metachain path:\t" + path);
        } else {
            DEBUG_COUT("Missing metachain path");
            print_config_file_params_and_exit();
        }
        if (config_json.HasMember("hash") && config_json["hash"].IsString()) {
            hash = config_json["hash"].GetString();
            auto hash_as_vec = metahash::crypto::hex2bin(hash);
            if (hash_as_vec.size() != 32) {
                DEBUG_COUT("Invalid trusted block hash");
                print_config_file_params_and_exit();
            }

            DEBUG_COUT("Trusted block hash:\t" + hash);
        } else {
            DEBUG_COUT("Missing trusted hash");
            print_config_file_params_and_exit();
        }
        if (config_json.HasMember("key") && config_json["key"].IsString()) {
            key = config_json["key"].GetString();

            std::vector<unsigned char> priv_k = metahash::crypto::hex2bin(key);
            std::vector<char> PubKey;
            if (!metahash::crypto::generate_public_key(PubKey, priv_k)) {
                DEBUG_COUT("Error while parsing Private key");
                print_config_file_params_and_exit();
            }

            std::array<char, 25> addres = metahash::crypto::get_address(PubKey);
            std::string Text_addres = "0x" + metahash::crypto::bin2hex(addres);
            DEBUG_COUT("got key for address:\t" + Text_addres);

        } else {
            DEBUG_COUT("Missing Private key");
            print_config_file_params_and_exit();
        }
        if (config_json.HasMember("cores") && config_json["cores"].IsArray()) {
            auto& v_list = config_json["cores"];

            for (uint i = 0; i < v_list.Size(); i++) {
                auto& record = v_list[i];

                if (record.HasMember("address") && record["address"].IsString()
                    && record.HasMember("host") && record["host"].IsString()
                    && record.HasMember("port") && record["port"].IsUint()
                    && record["port"].GetUint() != 0) {

                    DEBUG_COUT("Core\t" + std::string(record["address"].GetString()) + "\t" + std::string(record["host"].GetString()) + "\t" + std::to_string(record["port"].GetUint()));
                    core_list.insert({ record["address"].GetString(), { record["host"].GetString(), record["port"].GetUint() } });
                }
            }

            if (core_list.empty()) {
                DEBUG_COUT("WARNING: No cores present in configuration file");
            }
        } else {
            DEBUG_COUT("WARNING: No cores present in configuration file");
        }
    } else {
        DEBUG_COUT("Invalid Configuration File");
        print_config_file_params_and_exit();
    }
}