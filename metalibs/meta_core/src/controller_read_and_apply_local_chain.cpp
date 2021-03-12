#include "controller.hpp"

#include <meta_block.h>
#include <meta_constants.hpp>
#include <meta_log.hpp>
#include <rapidjson/document.h>

#include <experimental/filesystem>
#include <fstream>
#include <list>

namespace metahash::meta_core {

std::set<std::string> get_files_in_dir(const std::string& path)
{
    std::set<std::string> files;
    //                          y   m   d   t
    const uint file_name_size = 4 + 2 + 2 + 4;

    namespace fs = std::experimental::filesystem;
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
    std::list<std::future<block::Block*>>& futures,
    char* block_buff,
    int64_t block_size,
    bool delete_buff)
{
    auto promise = std::make_shared<std::promise<block::Block*>>();
    futures.emplace_back(promise->get_future());

    boost::asio::post(io_context, [block_buff, block_size, delete_buff, promise]() {
        std::string_view block_as_string(block_buff, block_size);
        auto* block = block::parse_block(block_as_string);

        if (block) {
            if (!dynamic_cast<block::CommonBlock*>(block) && !dynamic_cast<block::ApproveBlock*>(block)) {
                delete block;
                block = nullptr;
            } else {
                block->set_local();
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

std::string read_last_known_state(sha256_2& proved_block)
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

                DEBUG_COUT("got last state info file:\t" + last_file + "\t and block:\t" + last_block);
            }
        }

        last_known_state_file.close();
    }

    return last_file;
}

void read_stored_blocks(
    boost::asio::io_context& io_context,
    const std::string& path,
    const std::string& last_file,
    std::list<std::future<metahash::block::Block*>>& pending_data)
{
    uint files_read = 0;
    uint blocks_read = 0;

    char uint64_buff[8];
    std::set<std::string> files = get_files_in_dir(path);

    bool old_files = true;
    for (const std::string& file : files) {
        if (file == last_file) {
            old_files = false;
        }

        if (old_files) {
            continue;
        }

        std::ifstream ifile(file.c_str(), std::ios::in | std::ios::binary);

        if (ifile.is_open()) {
            files_read++;
            while (ifile.read(uint64_buff, 8)) {
                uint64_t block_size = *(reinterpret_cast<uint64_t*>(uint64_buff));
                char* block_buff = new char[block_size];

                if (ifile.read(block_buff, static_cast<int64_t>(block_size))) {
                    blocks_read++;
                    parse_block_async(io_context, pending_data, block_buff, block_size, true);
                } else {
                    DEBUG_COUT("read file error\t" + file);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    exit(1);
                }

                if (blocks_read % 250000 == 0) {
                    DEBUG_COUT("Read blocks\t" + std::to_string(blocks_read) + "\tin files\t " + std::to_string(files_read));
                }
            }

        } else {
            std::string msg = "!file.is_open()\t" + file;
            DEBUG_COUT(msg);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            exit(1);
        }
    }
}

void ControllerImplementation::read_and_apply_local_chain()
{
    auto last_file = read_last_known_state(proved_block);

    std::list<std::future<block::Block*>> pending_data;

    read_stored_blocks(io_context, path, last_file, pending_data);
    DEBUG_COUT("READ COMPLETE");

    uint blocks_processed = 0;
    for (auto&& fut : pending_data) {
        auto block = fut.get();
        if (block) {
            if (dynamic_cast<block::CommonBlock*>(block)) {
                blocks.insert(block);
            } else if (auto* a_block = dynamic_cast<block::ApproveBlock*>(block)) {
                for (auto& tx : a_block->get_txs()) {
                    block_approve[tx.get_block_hash()].insert({ "0x" + crypto::bin2hex(crypto::get_address(tx.pub_key)), new transaction::ApproveRecord(std::move(tx)) });
                }

                check_blocks();
            }
        }

        blocks_processed++;
        if (blocks_processed % 250000 == 0) {
            DEBUG_COUT("Processed blocks\t" + std::to_string(blocks_processed));
        }
    }
    DEBUG_COUT("PROCESS COMPLETE");

    check_blocks();

    DEBUG_COUT("LOCAL COMPLETE");
    {
        std::list<std::future<transaction::ApproveRecord*>> pending_data;

        sha256_2 got_block = last_applied_block;
        while (blocks.contains(got_block)) {
            auto approve_list_it = block_approve.find(got_block);
            if (approve_list_it == block_approve.end() || !approve_list_it->second.count(signer.get_mh_addr())) {
                auto promise = std::make_shared<std::promise<transaction::ApproveRecord*>>();
                pending_data.emplace_back(promise->get_future());

                boost::asio::post(io_context, [got_block, this, promise]() {
                    auto* p_ar = new transaction::ApproveRecord;
                    p_ar->make(got_block, signer);
                    p_ar->approve = true;

                    promise->set_value(p_ar);
                });
            }

            got_block = blocks[got_block]->get_prev_hash();
        }

        for (auto&& fut : pending_data) {
            auto p_ar = fut.get();
            if (!block_approve[p_ar->get_block_hash()].insert({ signer.get_mh_addr(), p_ar }).second) {
                delete p_ar;
            }
        }
    }
    DEBUG_COUT("APPROVE COMPLETE");
}

void ControllerImplementation::check_blocks()
{
    while (check_awaited_blocks())
        ;
}
}