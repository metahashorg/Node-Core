#include <utility>

#include <utility>
#include <vector>

#include <meta_log.hpp>

#include "core_controller.hpp"
#include "statics.hpp"

std::vector<std::string> split(const std::string& s, char delim)
{
    std::stringstream ss(s);
    std::string item;
    std::vector<std::string> elems;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::set<std::pair<std::string, int>> parse_core_list(const std::string& in_string)
{
    std::set<std::pair<std::string, int>> core_list;

    auto records = split(in_string, '\n');
    for (const auto& record : records) {
        auto host_port = split(record, ':');
        if (host_port.size() == 2) {
            core_list.insert({ host_port[0], std::stoi(host_port[1]) });
        }
    }

    return core_list;
}

size_t writefunc(void* ptr, size_t size, size_t nmemb, std::string* p_resp)
{
    std::string& resp = *p_resp;
    resp.insert(resp.end(), reinterpret_cast<char*>(ptr), reinterpret_cast<char*>(ptr) + nmemb);
    return size * nmemb;
}

bool CoreController::CoreConnection::curl_post(const std::string& request_method, const std::string& reques_string, std::string& response)
{
    if (!curl) {
        curl = curl_easy_init();
    }

    std::string full_url = "http://" + host_port.first + "/" + request_method;
    curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
    curl_easy_setopt(curl, CURLOPT_PORT, host_port.second);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, reques_string.size());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, reques_string.data());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    int ret = curl_easy_perform(curl);

    if (!ret) {
        return true;
    }

    curl_easy_cleanup(curl);
    curl = nullptr;

    return false;
}

CoreController::CoreConnection::CoreConnection(std::pair<std::string, int> _host)
    : host_port(std::move(_host))
    , curl(curl_easy_init())
{
}

std::string CoreController::CoreConnection::send_with_return(const std::string& request_method, const std::string& reques_string)
{
    std::string response;

    int counter = 0;
    int status = 0;
    while (status <= 0) {
        counter++;

        //        status = event.post_keep_alive(host_port.first, host_port.second, host_port.first, request_method, reques_string, response, 1000);
        response.clear();
        bool success = curl_post(request_method, reques_string, response);

        if (success) {
            break;
        }

        if (counter > 2) {
            DEBUG_COUT("counter > 2");
            return "";
        }
    }

    return response;
}

void CoreController::CoreConnection::start_loop(moodycamel::ConcurrentQueue<CoreController::Message*>& message_queue)
{
    std::thread([&message_queue, this]() {
        while (this->goon.load()) {
            CoreController::Message* msg = nullptr;
            if (message_queue.try_dequeue(msg)) {
                std::string response;
                int counter = 0;

                while (true) {
                    response.clear();
                    bool success = curl_post(msg->url, msg->msg, response);
                    if (success) {
                        if (msg->get_resp) {
                            msg->resp->clear();
                            msg->resp->insert(msg->resp->begin(), response.begin(), response.end());
                            msg->get_resp->store(true);
                        }

                        delete msg;
                        break;
                    }

                    uint64_t time_milli = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count());
                    if (msg->milli_time && msg->milli_time < time_milli) {
                        if (msg->get_resp) {
                            msg->resp->clear();
                            msg->get_resp->store(true);
                        }

                        delete msg;
                        break;
                    }

                    counter++;
                    if (counter > 10) {
                        message_queue.enqueue(msg);
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        break;
                    }
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }).detach();
}

CoreController::CoreController(const std::set<std::pair<std::string, int>>& core_list, std::pair<std::string, int> host_port)
    : my_host_port(std::move(host_port))
{
    for (const auto& host : core_list) {
        known_core_list.emplace(host, host);
    }
    sync_core_lists();
}

void CoreController::sync_core_lists()
{
    bool got_new = true;
    while (got_new) {
        auto resp = send_with_return(RPC_GET_CORE_LIST, "");

        core_lock.lock();
        for (const auto& resp_pair : resp) {
            auto hosts = parse_core_list(resp_pair.second);

            for (const auto& host_port : hosts) {
                known_core_list.emplace(host_port, host_port);
            }
        }

        got_new = false;
        for (auto& host : known_core_list) {
            bool connected = false;

            if (host.first == my_host_port) {
                connected = true;
            }

            for (const auto& core_pair : cores) {
                if (core_pair.second->host_port == host.first) {
                    connected = true;
                    break;
                }
            }

            if (!connected) {
                std::string my_addr = my_host_port.first + ":" + std::to_string(my_host_port.second);

                std::string addr = host.second.send_with_return(RPC_GET_CORE_ADDR, my_addr);

                DEBUG_COUT("Got address " + addr + " from " + host.first.first + ":" + std::to_string(host.first.second));

                if (addr.size() == 52) {
                    host.second.start_loop(message_queue[addr]);
                    cores.insert({ addr, &(host.second) });
                    got_new = true;
                } else {
                    DEBUG_COUT("Bad address");
                    DEBUG_COUT(addr);
                }
            }
        }
        core_lock.unlock();
    }
}

void CoreController::add_core(
    std::string& host,
    uint64_t port)
{
    std::lock_guard lock(core_lock);
    std::pair<std::string, int> host_port = { host, port };

    known_core_list.emplace(host_port, host_port);
}

std::string CoreController::get_core_list()
{
    std::lock_guard lock(core_lock);

    std::string core_list_as_str;

    for (const auto& host : known_core_list) {
        core_list_as_str += host.first.first + ":" + std::to_string(host.first.second) + "\n";
    }

    return core_list_as_str;
}

void CoreController::send_no_return(const std::string& req, const std::string& data)
{
    std::lock_guard lock(core_lock);

    std::set<std::string> addresses;

    for (const auto& core_pair : cores) {
        if (addresses.insert(core_pair.first).second) {
            message_queue[core_pair.first].enqueue(new Message{ req, data, 0, nullptr, nullptr });
        }
    }
}

std::map<std::string, std::string> CoreController::send_with_return(const std::string& req, const std::string& data)
{
    core_lock.lock();

    std::map<std::string, std::string> resp_strings;

    uint64_t time_milli = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count());
    time_milli += 3000;

    std::set<std::string> addresses;

    std::map<std::string, std::pair<std::atomic<bool>, std::string>> responses;
    for (const auto& core_pair : cores) {
        if (addresses.insert(core_pair.first).second) {
            responses[core_pair.first].first = false;
            message_queue[core_pair.first].enqueue(new Message{ req, data, time_milli, &(responses[core_pair.first].first), &(responses[core_pair.first].second) });
        }
    }

    core_lock.unlock();

    for (const auto& response : responses) {
        while (!response.second.first.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        resp_strings[response.first] = response.second.second;
    }

    return resp_strings;
}

void CoreController::send_no_return_to_core(
    const std::string& addr,
    const std::string& req,
    const std::string& data)
{
    std::lock_guard lock(core_lock);

    for (const auto& core_pair : cores) {
        if (core_pair.first == addr) {
            message_queue[core_pair.first].enqueue(new Message{ req, data, 0, nullptr, nullptr });
            return;
        }
    }
}

std::string CoreController::send_with_return_to_core(
    const std::string& addr,
    const std::string& req,
    const std::string& data)
{
    core_lock.lock();

    uint64_t time_milli = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count());
    if (req == RPC_GET_CHAIN) {
        time_milli += 30000;
    } else {
        time_milli += 3000;
    }

    std::atomic<bool> got_resp = false;
    std::string resp_str;

    for (const auto& core_pair : cores) {
        if (core_pair.first == addr) {
            message_queue[core_pair.first].enqueue(new Message{ req, data, time_milli, &got_resp, &resp_str });
            break;
        }
    }

    core_lock.unlock();

    while (!got_resp.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return resp_str;
}
