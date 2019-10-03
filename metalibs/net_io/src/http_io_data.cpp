//
//  http_io_data.cpp
//  net_lib_example
//
//  Created by Dmitriy Borisenko on 10.10.2017.
//
//

#include "http_io_data.hpp"

#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <uriparser/Uri.h>

HTTP_SERVER_IO::HTTP_SERVER_IO()
{
    wait_read = true;
    wait_write = false;
}

bool HTTP_SERVER_IO::read_complete()
{
    switch (req_parse_state) {
    case 0: {
        const char* method = nullptr;
        const char* path = nullptr;
        int minor_version = 0;
        size_t method_len = 0, path_len = 0;
        size_t num_headers = NUM_HEADERS;

        header_size = phr_parse_request(read_buff.data(), read_buff.size(),
            &method, &method_len,
            &path, &path_len,
            &minor_version,
            headers, &num_headers,
            0);

        if (header_size < 0) {
            if (header_size == -1) {
                //                read_buff.clear();
                //                return true;
            } else {
                return false;
            }
        }

        if (read_buff.empty()) {
            return false;
        }

        if (method) {
            req_method = std::string(method, method_len);
        } else {
            req_method = "";
        }

        for (uint i = 0; i < num_headers; i++) {
            std::string name(headers[i].name, headers[i].name_len);
            std::string value(headers[i].value, headers[i].value_len);
            req_headers.insert({ name, value });
        }

        req_url = "http://";
        if (req_headers.find("Host") != req_headers.end()) {
            req_url += req_headers["Host"];
        }
        if (path) {
            req_path = std::string(path, path_len);
            req_url += "/";
            req_url += std::string(path, path_len);
        }

        if (req_headers.find("Content-Length") != req_headers.end()) {
            content_length = std::atoi(req_headers["Content-Length"].c_str());
        } else if (req_headers.find("Content-length") != req_headers.end()) {
            content_length = std::atoi(req_headers["Content-length"].c_str());
        }
        req_parse_state = 1;
    };
        [[fallthrough]];
    case 1: {
        if (content_length > (int(read_buff.size()) - header_size)) {
            return false;
        }

        req_parse_state = 2;
    };
        [[fallthrough]];
    case 2: {
        req_parse_state = 0;

        if (header_size < int(read_buff.size())) {
            req_post = std::string(read_buff.begin() + header_size, read_buff.end());
        } else {
            req_post = "";
        }

        if (!req_url.empty()) {
            UriParserStateA state;
            UriUriA uriParse;
            UriQueryListA *query_list, *it;
            int item_count, i = 0;

            state.uri = &uriParse;
            if (uriParseUriA(&state, req_url.c_str()) != URI_SUCCESS) {
                uriFreeUriMembersA(&uriParse);
                break;
            }

            if (uriDissectQueryMallocA(&query_list, &item_count, uriParse.query.first, uriParse.query.afterLast) != URI_SUCCESS) {
                uriFreeUriMembersA(&uriParse);
                break;
            }

            if (!query_list) {
                uriFreeUriMembersA(&uriParse);
                break;
            }

            for (it = query_list; i < item_count && it != nullptr; it = it->next, i++) {
                if (it->key) {
                    if (it->value) {
                        req_params.insert({ it->key, it->value });
                    } else {
                        req_params.insert({ it->key, "" });
                    }
                }
            }

            uriFreeQueryListA(query_list);
            uriFreeUriMembersA(&uriParse);
        }
    };
        //        [[fallthrough]];
    }

    _fn_on_read(dynamic_cast<SOCKET_IO_DATA*>(this));
    make_response();
    wait_read = false;
    wait_write = true;
    p_TP->runSheduled(1, &SOCKET_IO_DATA::write_data, this);

    return true;
}

void HTTP_SERVER_IO::write_complete()
{
    //    if (keep_alive) {
    wait_read = true;
    wait_write = false;
    clear();
    //    io_service->write_data(this);
    //    } else {
    //        io_service.close_connection(this);
    //    }
}

void HTTP_SERVER_IO::socket_closed()
{
    //    std::cerr << "Этот объект не является объектом типа B" << std::endl;
}

void HTTP_SERVER_IO::clear()
{
    read_buff.clear();
    write_buff.clear();

    std::memset(headers, 0, sizeof(headers));

    req_host.clear();
    req_method.clear();
    req_url.clear();
    req_path.clear();
    req_version.clear();
    req_post.clear();
    req_headers.clear();
    req_cookies.clear();
    req_params.clear();

    resp_code = 200;
    resp_data.clear();
    resp_headers.clear();
    resp_cookies.clear();

    req_parse_state = 0;
}

void HTTP_SERVER_IO::make_response()
{
    //std::cout << "HTTP_REQUEST_IO::make_response" << std::endl;
    static const std::string http_ver_p1 = "HTTP/1.1 ";
    static const std::string http_ver_p2 = " OK\r\nVersion: HTTP/1.1\r\n";

    static const std::string resp_end = "Content-Type: text/html; charset=utf-8\r\nContent-Length: ";
    static const std::string rnrn = "\r\n\r\n";

    static const std::string rn = "\r\n";
    static const std::string ddot = ": ";

    write_buff.clear();

    write_buff.insert(write_buff.end(), http_ver_p1.begin(), http_ver_p1.end());

    std::string code_str = std::to_string(resp_code);
    write_buff.insert(write_buff.end(), code_str.begin(), code_str.end());

    write_buff.insert(write_buff.end(), http_ver_p2.begin(), http_ver_p2.end());

    for (std::pair<std::string, std::string> header_pair : resp_headers) {
        write_buff.insert(write_buff.end(), header_pair.first.begin(), header_pair.first.end());
        write_buff.insert(write_buff.end(), ddot.begin(), ddot.end());
        write_buff.insert(write_buff.end(), header_pair.second.begin(), header_pair.second.end());
        write_buff.insert(write_buff.end(), rn.begin(), rn.end());
    }

    std::string data_size_str = std::to_string(resp_data.size());

    write_buff.insert(write_buff.end(), resp_end.begin(), resp_end.end());
    write_buff.insert(write_buff.end(), data_size_str.begin(), data_size_str.end());
    write_buff.insert(write_buff.end(), rnrn.begin(), rnrn.end());
    write_buff.insert(write_buff.end(), resp_data.begin(), resp_data.end());
}

void http_server(ThreadPool& TP,
    int listen_port,
    const std::function<void(HTTP_SERVER_IO*)>& _fn)
{
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::function<void(SOCKET_IO_DATA*)> process_function = [_fn](SOCKET_IO_DATA* data) {
        _fn(dynamic_cast<HTTP_SERVER_IO*>(data));
    };

    IO_SERVICE io_service(TP);
    SocketServer SS(TP, io_service, listen_port, []() {
        return dynamic_cast<SOCKET_IO_DATA*>(new HTTP_SERVER_IO);
    });

    SS.set_read_coplete_action(process_function);

    SS.start();

    while (true) {
        std::this_thread::sleep_for(std::chrono::hours(24));
    }

    SS.stop();
};
/*
bool HTTP_CLIENT_IO::read_complete()
{
    switch (req_parse_state) {
    case 0: {
        const char* msg = nullptr;
        int minor_version;
        size_t msg_len = 0, num_headers = 0;

        num_headers = 64;
        header_size = phr_parse_response(read_buff.data(), read_buff.size(), &minor_version, &resp_code, &msg, &msg_len, headers, &num_headers, 0);

        if (header_size == -1) {
            read_buff.clear();
            return true;
        }
        if (header_size < 0) {
            return false;
        }

        num_headers = 0;

        if (read_buff.empty()) {
            return true;
        }

        for (uint i = 0; i < num_headers; i++) {
            std::string name(headers[i].name, headers[i].name_len);
            std::string value(headers[i].value, headers[i].value_len);
            resp_headers.insert({ name, value });
        }

        int content_length = 0;

        if (req_headers.find("Content-Length") != req_headers.end()) {
            content_length = std::atoi(req_headers["Content-Length"].c_str());
        } else if (req_headers.find("Content-length") != req_headers.end()) {
            content_length = std::atoi(req_headers["Content-length"].c_str());
        }

        //std::cout << "req_parse_state = 1" << std::endl;
        req_parse_state = 1;
    };
        [[fallthrough]];
    case 1: {
        if (content_length > (int(read_buff.size()) - header_size)) {
            //std::cout << "content_length > (int(read_buff.size()) - header_size)" << std::endl;
            return false;
        }
        //std::cout << "req_parse_state = 2" << std::endl;

        req_parse_state = 2;
    };
        [[fallthrough]];
    case 2: {
        req_parse_state = 0;

        if (header_size < int(read_buff.size())) {
            resp_data = std::string(read_buff.begin() + header_size, read_buff.end());
        } else {
            resp_data = "";
        }
        //std::cout << "URI Parsed" << std::endl;
    };
        //        [[fallthrough]];
    }

    _fn_on_read(dynamic_cast<SOCKET_IO_DATA*>(this));

    clear();

    return true;
}

void HTTP_CLIENT_IO::write_complete()
{
    //    if (keep_alive) {
    io_service->read_data(this);
    //    } else {
    //        io_service.close_connection(this);
    //    }
}

void HTTP_CLIENT_IO::socket_closed()
{
}

void HTTP_CLIENT_IO::clear()
{
    read_buff.clear();
    write_buff.clear();

    req_host.clear();
    req_method.clear();
    req_url.clear();
    req_version.clear();
    req_post.clear();
    req_headers.clear();
    req_cookies.clear();
    req_params.clear();
    req_params_multi.clear();

    resp_code = 200;
    resp_data.clear();
    resp_headers.clear();
    resp_cookies.clear();
}

void HTTP_CLIENT_IO::make_request()
{
    static const std::string http_ver_p1 = "POST ";
    static const std::string http_ver_p2 = " HTTP/1.1\r\n";

    static const std::string host = "Host: ";

    static const std::string connection = "Connection: Keep-Alive\r\n";

    static const std::string req_end = "Content-Type: application/json; charset=utf-8\r\nContent-Length: ";
    static const std::string rnrn = "\r\n\r\n";

    static const std::string rn = "\r\n";
    static const std::string ddot = ": ";

    write_buff.clear();

    write_buff.insert(write_buff.end(), http_ver_p1.begin(), http_ver_p1.end());
    write_buff.insert(write_buff.end(), req_url.begin(), req_url.end());
    write_buff.insert(write_buff.end(), http_ver_p2.begin(), http_ver_p2.end());
    write_buff.insert(write_buff.end(), host.begin(), host.end());
    write_buff.insert(write_buff.end(), req_host.begin(), req_host.end());
    write_buff.insert(write_buff.end(), rn.begin(), rn.end());

    for (std::pair<std::string, std::string> header_pair : req_headers) {
        write_buff.insert(write_buff.end(), header_pair.first.begin(), header_pair.first.end());
        write_buff.insert(write_buff.end(), ddot.begin(), ddot.end());
        write_buff.insert(write_buff.end(), header_pair.second.begin(), header_pair.second.end());
        write_buff.insert(write_buff.end(), rn.begin(), rn.end());
    }

    write_buff.insert(write_buff.end(), connection.begin(), connection.end());

    std::string data_size_str = std::to_string(req_post.size());

    write_buff.insert(write_buff.end(), req_end.begin(), req_end.end());
    write_buff.insert(write_buff.end(), data_size_str.begin(), data_size_str.end());
    write_buff.insert(write_buff.end(), rnrn.begin(), rnrn.end());
    write_buff.insert(write_buff.end(), req_post.begin(), req_post.end());
}
*/
