#include "controller.hpp"
#include "block.h"

#include <statics.hpp>
#include <meta_log.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <experimental/filesystem>
#include <fstream>

namespace metahash::metachain {

std::set<std::string> get_files_in_dir(std::string& path)
{
    namespace fs = std::experimental::filesystem;

    std::set<std::string> files;

    //                          y   m   d   t
    const uint file_name_size = 4 + 2 + 2 + 4;

    for (const auto& p : fs::directory_iterator(path)) {
        const auto& file_path = p.path();
        auto filename = file_path.filename().string();

        if (filename.length() == file_name_size &&
            // Year
            std::isdigit(filename[0]) && std::isdigit(filename[1]) && std::isdigit(filename[2]) && std::isdigit(filename[3]) &&
            // Month
            std::isdigit(filename[4]) && std::isdigit(filename[5]) &&
            // Day
            std::isdigit(filename[6]) && std::isdigit(filename[7]) &&
            // Extension
            filename.compare(8, 4, std::string { ".blk" }) == 0) {
            files.insert(file_path.string());
        }
    }

    return files;
}

void parse_block_async(
    boost::asio::io_context& io_context,
    std::list<std::future<Block*>>& futures,
    char* block_buff,
    int64_t block_size,
    bool delete_buff)
{
    auto promise = std::make_shared<std::promise<Block*>>();
    futures.emplace_back(promise->get_future());

    boost::asio::post(io_context, [block_buff, block_size, delete_buff, promise]() {
        std::string_view block_as_string(block_buff, block_size);
        Block* block = parse_block(block_as_string);

        if (block) {
            if (!dynamic_cast<CommonBlock*>(block)) {
                delete block;
                block = nullptr;
            }
        } else {
            DEBUG_COUT("Block parse error");
        }

        if (delete_buff) {
            delete[] block_buff;
        }

        promise->set_value(block);
    });
}


void ControllerImplementation::read_and_apply_local_chain()
{
    std::string last_file;

    std::ifstream last_known_state_file("last_state.json");
    if (last_known_state_file.is_open()) {
        std::string content((std::istreambuf_iterator<char>(last_known_state_file)), (std::istreambuf_iterator<char>()));

        rapidjson::Document last_known_state_json;
        if (!last_known_state_json.Parse(content.c_str()).HasParseError()) {

            if (last_known_state_json.HasMember("hash") && last_known_state_json["hash"].IsString()
                && last_known_state_json.HasMember("file") && last_known_state_json["file"].IsString()) {

                last_file = std::string(last_known_state_json["file"].GetString(), last_known_state_json["file"].GetStringLength());
                std::string last_block = std::string(last_known_state_json["hash"].GetString(), last_known_state_json["hash"].GetStringLength());
                std::vector<unsigned char> bin_proved_hash = crypto::hex2bin(last_block);
                std::copy_n(bin_proved_hash.begin(), 32, proved_block.begin());

                DEBUG_COUT("got last state info. file:\t" + last_file + "\t and block:\t" + last_block);
            }
        }

        last_known_state_file.close();
    }

    std::list<std::future<Block*>> pending_data;
    std::unordered_map<sha256_2, Block*, crypto::Hasher> prev_tree;

    char uint64_buff[8];
    std::set<std::string> files = get_files_in_dir(path);

    for (const std::string& file : files) {
        std::ifstream ifile(file.c_str(), std::ios::in | std::ios::binary);

        if (ifile.is_open()) {
            while (ifile.read(uint64_buff, 8)) {
                uint64_t block_size = *(reinterpret_cast<uint64_t*>(uint64_buff));
                char* block_buff = new char[block_size];

                if (ifile.read(block_buff, static_cast<int64_t>(block_size))) {
                    parse_block_async(io_context, pending_data, block_buff, block_size, true);
                } else {
                    std::string msg = "read file error\t" + file;
                    DEBUG_COUT(msg);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    exit(1);
                }
            }
        } else {
            std::string msg = "!file.is_open()\t" + file;
            DEBUG_COUT(msg);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            exit(1);
        }
    }

    for (auto&& fut : pending_data) {
        auto block = fut.get();
        if (block) {
            if (!blocks.insert({ block->get_block_hash(), block }).second) {
                DEBUG_COUT("Duplicate block in chain\t" + crypto::bin2hex(block->get_block_hash()));
                delete block;
            } else if (!prev_tree.insert({ block->get_prev_hash(), block }).second) {
                DEBUG_COUT("Branches in block chain\t" + crypto::bin2hex(block->get_prev_hash()) + "\t->\t" + crypto::bin2hex(block->get_block_hash()));
                blocks.erase(block->get_block_hash());
                delete block;
            }
        }
    }

    DEBUG_COUT("PARSE BLOCKCHAIN COMPLETE");

    apply_block_chain(blocks, prev_tree, "local storage", false);

    DEBUG_COUT("READ BLOCK COMPLETE");
}

void ControllerImplementation::write_block(Block* block)
{
    auto theTime = static_cast<time_t>(block->get_block_timestamp());
    struct tm* aTime = localtime(&theTime);

    int day = aTime->tm_mday;
    int month = aTime->tm_mon + 1; // Month is 0 - 11, add 1 to get a jan-dec 1-12 concept
    int year = aTime->tm_year + 1900; // Year is # years since 1900

    //                          y   m   d   t
    const uint file_name_size = 38; //4 + 2 + 2 + 4 + 1;
    char file_name[file_name_size] = { 0 };
    std::snprintf(file_name, file_name_size, "%04d%02d%02d.blk", year, month, day);

    std::string file_path = path + "/" + file_name;

    DEBUG_COUT("file_path\t" + file_path);

    if (dynamic_cast<CommonBlock*>(block)) {
        DEBUG_COUT("CommonBlock");

        prev_timestamp = block->get_block_timestamp();
        last_applied_block = block->get_block_hash();

        prev_day = prev_timestamp / DAY_IN_SECONDS;
        prev_state = block->get_block_type();

        uint64_t block_size = block->get_data().size() /* + approve_buff.size()*/;

        std::ofstream myfile;
        myfile.open(file_path.c_str(), std::ios::out | std::ios::app | std::ios::binary);
        myfile.write(reinterpret_cast<char*>(&block_size), sizeof(uint64_t));
        myfile.write(block->get_data().data(), static_cast<int64_t>(block->get_data().size()));
        myfile.close();

        {
            auto approve_block = new ApproveBlock;
            std::vector<ApproveRecord*> approve_list;
            for (const auto& tx_pair : block_approve[block->get_block_hash()]) {
                approve_list.push_back(tx_pair.second);
            }
            if (approve_block->make(block->get_block_timestamp(), block->get_block_hash(), approve_list)) {
                uint64_t approve_block_size = approve_block->get_data().size() /* + approve_buff.size()*/;
                std::ofstream out_file;
                out_file.open(file_path.c_str(), std::ios::out | std::ios::app | std::ios::binary);
                out_file.write(reinterpret_cast<char*>(&approve_block_size), sizeof(uint64_t));
                out_file.write(approve_block->get_data().data(), static_cast<int64_t>(approve_block->get_data().size()));
                out_file.close();
                delete approve_block;
            }
        }

        {
            uint64_t timestamp = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count());
            DEBUG_COUT("block size and latency\t" + std::to_string(dynamic_cast<CommonBlock*>(block)->get_txs().size()) + "\t" + std::to_string(timestamp - prev_timestamp));
        }

        {
            auto common_block = dynamic_cast<CommonBlock*>(block);
            if (common_block && common_block->get_block_type() == BLOCK_TYPE_STATE) {
                std::ofstream cache_file("last_state.json");
                if (cache_file.is_open()) {
                    rapidjson::StringBuffer s;
                    rapidjson::Writer<rapidjson::StringBuffer> writer(s);
                    writer.StartObject();
                    {
                        writer.String("hash");
                        writer.String(crypto::bin2hex(common_block->get_block_hash()).c_str());
                        writer.String("file");
                        writer.String(file_path.c_str());
                    }
                    writer.EndObject();

                    cache_file << std::string(s.GetString());
                    cache_file.close();
                }
            }
        }
    } else if (dynamic_cast<RejectedTXBlock*>(block)) {
        DEBUG_COUT("RejectedTXBlock");

        uint64_t block_size = block->get_data().size() /* + approve_buff.size()*/;

        std::ofstream out_file;
        out_file.open(file_path.c_str(), std::ios::out | std::ios::app | std::ios::binary);
        out_file.write(reinterpret_cast<char*>(&block_size), sizeof(uint64_t));
        out_file.write(block->get_data().data(), static_cast<int64_t>(block->get_data().size()));
        out_file.close();
    }
}

}