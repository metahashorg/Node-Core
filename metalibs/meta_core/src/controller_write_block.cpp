#include "controller.hpp"
#include <meta_block.h>
#include <meta_constants.hpp>
#include <meta_log.hpp>
#include <rapidjson/writer.h>

#include <fstream>

namespace metahash::meta_core {

void ControllerImplementation::write_block(block::Block* block)
{
    if (block->is_local()) {
        return;
    }

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

    if (dynamic_cast<block::CommonBlock*>(block)) {
        DEBUG_COUT("CommonBlock");

        uint64_t block_size = block->get_data().size() /* + approve_buff.size()*/;

        std::ofstream myfile;
        myfile.open(file_path.c_str(), std::ios::out | std::ios::app | std::ios::binary);
        myfile.write(reinterpret_cast<char*>(&block_size), sizeof(uint64_t));
        myfile.write(block->get_data().data(), static_cast<int64_t>(block->get_data().size()));
        myfile.close();

        {
            auto approve_block = new block::ApproveBlock;
            std::vector<transaction::ApproveRecord*> approve_list;
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
            DEBUG_COUT("block size and latency\t" + std::to_string(dynamic_cast<block::CommonBlock*>(block)->get_data().size()) + "\t" + std::to_string(timestamp - prev_timestamp));
        }

        {
            auto common_block = dynamic_cast<block::CommonBlock*>(block);
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
    } else if (dynamic_cast<block::RejectedTXBlock*>(block)) {
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