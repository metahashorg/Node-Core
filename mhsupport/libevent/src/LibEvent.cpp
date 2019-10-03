#include <event2/event-config.h>
#include <event2/thread.h>
#include <event2/util.h>
#include <evhttp.h>
#include "LibEvent.h"

namespace mh::libevent {

using std::string;
using std::unordered_map;

namespace {

struct req_data
{
    event_base* base = nullptr;
    string response;
    int code = 0;
};

void _reqhandler(struct evhttp_request* req, void* state)
{
    struct evbuffer* buf;
    char cbuf[8192];

    auto* data = (req_data*)state;
    data->response.clear();


    if (!req) {
        data->code = -1;
    }
    else if (req->response_code == 0) {
        data->code = 0;
    }
    else if (req->response_code != 200) {
        data->code = req->response_code;
    }
    else {
        data->code = 200;
        buf = req->input_buffer;

        if (buf) {
            while (evbuffer_get_length(buf)) {
                int n;
                n = evbuffer_remove(buf, cbuf, sizeof(cbuf));
                if (n > 0) {
                    data->response.append(cbuf, n);
                }
            }
        }
    }

    event_base_loopexit(data->base, nullptr);
}

} // namespace

LibEvent::LibEvent() : evbase(event_base_new())
{
    if (!evbase)
        throw std::runtime_error{"evbase is nullptr"};
}

LibEvent::~LibEvent()
{
    event_base_free(evbase);
    evbase = nullptr;

    for (auto& conn : conns)
        if (conn.second)
            evhttp_connection_free(conn.second);
}

string LibEvent::get(const string& host, uint16_t port, const string& hostname, const string& path)
{
    string response;
    int code = 0;

    get_core(host, port, hostname, path, 0, response, code);

    return response;
}

bool LibEvent::get_core(const string& host, uint16_t port, const string& hostname,
                        const string& path, int32_t timeout_ms, string& response, int& code)
{
    if (host.empty() || !port || hostname.empty() || path.empty())
        return false;

    req_data data;
    data.base = evbase;


    struct evhttp_connection* conn = nullptr;
    struct evhttp_request* req = nullptr;

    conn = evhttp_connection_base_new(evbase, nullptr, host.c_str(), port);
    if (timeout_ms) {
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms * 1000) - (timeout_ms / 1000) * 1000000;

        evhttp_connection_set_timeout_tv(conn, &tv);
    }

    req = evhttp_request_new(_reqhandler, (void*)&data);
    evhttp_add_header(req->output_headers, "Host", hostname.c_str());
    evhttp_add_header(req->output_headers, "Content-Length", "0");

    evhttp_make_request(conn, req, EVHTTP_REQ_GET, path.c_str());
    event_base_dispatch(evbase);
    evhttp_connection_free(conn);

    response = data.response;
    code = data.code;

    return data.code == 200;
}

int LibEvent::post_core(const string& host, uint16_t port, const string& hostname,
                        const string& path, const string& post_data, string& response,
                        int32_t timeout_ms)
{
    if (host.empty() || !port || hostname.empty() || path.empty())
        return 500;

    req_data data;
    data.base = evbase;

    struct evhttp_connection* conn = nullptr;
    struct evhttp_request* req = nullptr;

    conn = evhttp_connection_base_new(evbase, nullptr, host.c_str(), port);

    if (timeout_ms) {
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms * 1000) - (timeout_ms / 1000) * 1000000;

        evhttp_connection_set_timeout_tv(conn, &tv);
    }


    req = evhttp_request_new(_reqhandler, (void*)&data);
    evhttp_add_header(req->output_headers, "Host", hostname.c_str());
    evhttp_add_header(req->output_headers, "Content-Length",
                      std::to_string(post_data.size()).c_str());

    if (!post_data.empty())
        evbuffer_add(req->output_buffer, post_data.c_str(), post_data.size());

    evhttp_make_request(conn, req, EVHTTP_REQ_POST, path.c_str());
    event_base_dispatch(evbase);
    evhttp_connection_free(conn);

    response = data.response;

    return data.code;
}

int LibEvent::post_keep_alive(const string& host, uint16_t port, const string& hostname,
                              const string& path, const string& post_data, string& response,
                              int32_t timeout_ms)
{
    if (host.empty() || !port || hostname.empty() || path.empty())
        return 500;

    req_data data;
    data.base = evbase;

    string key = host + ":" + std::to_string(port) + ":" + hostname;
    struct evhttp_connection* conn = nullptr;
    {
        auto it = conns.find(key);
        if (it != conns.end()) {
            conn = it->second;
        }
        else {
            conn = evhttp_connection_base_new(evbase, nullptr, host.c_str(), port);
            conns[key] = conn;
        }
    }

    if (!conn)
        return 500;

    if (timeout_ms) {
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms * 1000) - (timeout_ms / 1000) * 1000000;

        evhttp_connection_set_timeout_tv(conn, &tv);
    }


    struct evhttp_request* req = evhttp_request_new(_reqhandler, (void*)&data);
    if (!req)
        return 500;

    evhttp_add_header(req->output_headers, "Host", hostname.c_str());
    evhttp_add_header(req->output_headers, "Content-Length",
                      std::to_string(post_data.size()).c_str());
    evhttp_add_header(req->output_headers, "Connection", "Keep-Alive");

    if (!post_data.empty()) {
        if (evbuffer_add(req->output_buffer, post_data.c_str(), post_data.size()) != 0) {
            evhttp_request_free(req);
            return 500;
        }
    }

    if (evhttp_make_request(conn, req, EVHTTP_REQ_POST, path.c_str()) == 0) {
        if (event_base_dispatch(evbase) == 0) {
            response = data.response;
        }
    }

    return data.code;
}

} // namespace mh::libevent
