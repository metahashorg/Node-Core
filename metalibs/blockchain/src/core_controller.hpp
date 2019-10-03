#ifndef CORE_CONTROLLER_HPP
#define CORE_CONTROLLER_HPP

#include <map>
#include <mutex>
#include <set>

#include <curl/curl.h>

//#include <LibEvent.h>
#include <concurrentqueue.h>

class CoreController {
private:
    struct Message {
        std::string url;
        std::string msg;
        uint64_t milli_time;
        std::atomic<bool>* get_resp;
        std::string* resp;
    };

    struct CoreConnection {
        CoreConnection(std::pair<std::string, int>);

        std::atomic<bool> goon = true;
        //        mh::libevent::LibEvent event;
        CURL* curl = nullptr;

        std::pair<std::string, int> host_port;

        std::string send_with_return(const std::string& request_method, const std::string& reques_string);

        void start_loop(moodycamel::ConcurrentQueue<Message*>&);

        bool curl_post(const std::string& request_method, const std::string& reques_string, std::string& response);
    };

    std::mutex core_lock;

    std::multimap<std::string, CoreConnection*> cores;
    std::map<std::string, moodycamel::ConcurrentQueue<Message*>> message_queue;

    std::map<std::pair<std::string, int>, CoreConnection> known_core_list;

    std::pair<std::string, int> my_host_port;

public:
    CoreController(const std::set<std::pair<std::string, int>>&, std::pair<std::string, int>);

    void sync_core_lists();

    void add_core(std::string& host, uint64_t port);
    std::string get_core_list();

    void send_no_return(const std::string& req, const std::string& data);
    std::map<std::string, std::string> send_with_return(const std::string& req, const std::string& data);

    void send_no_return_to_core(const std::string& addr, const std::string& req, const std::string& data);
    std::string send_with_return_to_core(const std::string& addr, const std::string& req, const std::string& data);
};

#endif // CORE_CONTROLLER_HPP
