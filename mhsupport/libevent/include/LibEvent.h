#ifndef LIBEVENT_LIBEVENT_H_
#define LIBEVENT_LIBEVENT_H_

#include <evhttp.h>
#include <string>
#include <unordered_map>

namespace mh::libevent {

class LibEvent final
{
public:
    LibEvent();
    virtual ~LibEvent();

    std::string get(const std::string& host, uint16_t port, const std::string& hostname,
                    const std::string& path);

    bool get_core(const std::string& host, uint16_t port, const std::string& hostname,
                  const std::string& path, int32_t timeout_ms, std::string& response, int& code);

    int post_core(const std::string& host, uint16_t port, const std::string& hostname,
                  const std::string& path, const std::string& post_data, std::string& response,
                  int32_t timeout_ms = 0);

    int post_keep_alive(const std::string& host, uint16_t port, const std::string& hostname,
                        const std::string& path, const std::string& post_data,
                        std::string& response, int32_t timeout_ms = 0);

private:
    struct event_base* evbase = nullptr;
    std::unordered_map<std::string, evhttp_connection*> conns;
};

} // namespace mh::libevent

#endif /* LIBEVENT_LIBEVENT_H_ */
