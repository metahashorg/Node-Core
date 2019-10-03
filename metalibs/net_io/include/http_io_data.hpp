#ifndef http_io_data_hpp
#define http_io_data_hpp

#include <map>
#include <string>
#include <vector>

#include "picohttpparser.h"
#include "socket_server.hpp"

struct HTTP_SERVER_IO : SOCKET_IO_DATA {
    int req_parse_state = 0;
    int header_size = 0;

    static const size_t NUM_HEADERS = 16;
    phr_header headers[NUM_HEADERS]{};

    std::string req_host;
    std::string req_method;
    std::string req_url;
    std::string req_path;
    std::string req_version;
    int64_t content_length = 0;
    std::string req_post;
    std::map<std::string, std::string> req_headers;
    std::map<std::string, std::string> req_cookies;
    std::map<std::string, std::string> req_params;

    int64_t resp_code = 200;
    std::string resp_data;
    std::map<std::string, std::string> resp_headers;
    std::map<std::string, std::string> resp_cookies;

    bool keep_alive = true;

    bool read_complete() final;

    void write_complete() final;

    void socket_closed() final;

    void clear();

    void make_response();

    HTTP_SERVER_IO();
};

void http_server(ThreadPool& TP,
    int listen_port,
    const std::function<void(HTTP_SERVER_IO*)>& _fn);
/*
struct HTTP_CLIENT_IO : SOCKET_IO_DATA {
    int req_parse_state = 0;
    int header_size = 0;

    static const size_t NUM_HEADERS = 16;
    phr_header headers[NUM_HEADERS];

    std::string req_host;
    std::string req_method;
    std::string req_url;
    std::string req_version;
    int64_t content_length = 0;
    std::string req_post;
    std::map<std::string, std::string> req_headers;
    std::map<std::string, std::string> req_cookies;
    std::map<std::string, std::string> req_params;
    std::multimap<std::string, std::string> req_params_multi;

    int resp_code = 200;
    std::string resp_data;
    std::map<std::string, std::string> resp_headers;
    std::map<std::string, std::string> resp_cookies;

    bool keep_alive;

    char pading[7];

    bool read_complete() override;

    void write_complete() override;

    void socket_closed() override;

    void clear();

    void make_request();
};
*/
#endif /* http_io_data_hpp */
