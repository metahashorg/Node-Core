#ifndef SOCKET_IO_DATA_HPP
#define SOCKET_IO_DATA_HPP

#include <thread_pool.hpp>

struct SOCKET_IO_DATA {
    ThreadPool* p_TP = nullptr;

    std::atomic<bool> in_use = false;

    uint64_t timeout_prev_check = 0;
    uint64_t timeout = 0;

    int sock = 0;
    std::vector<char> read_buff;
    std::vector<char> write_buff;
    uint64_t write_data_complete = 0;
    std::vector<char> buff;

    bool wait_read = false;
    bool wait_write = false;

    virtual bool read_complete() = 0;
    virtual void write_complete() = 0;
    virtual void socket_closed() = 0;

    std::function<void(SOCKET_IO_DATA*)> _fn_on_read;
    std::function<void(SOCKET_IO_DATA*)> _fn_on_write;
    std::function<void(SOCKET_IO_DATA*)> _fn_on_close;

    SOCKET_IO_DATA();
    virtual ~SOCKET_IO_DATA() = default;

    void close_connection();
    void read_data();
    void write_data();

    void check_timeout(uint64_t timeout);

    bool try_lock();
};

#endif
