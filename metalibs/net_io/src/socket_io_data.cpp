#include <socket_io_data.hpp>

#include <sys/socket.h>
#include <unistd.h>

SOCKET_IO_DATA::SOCKET_IO_DATA()
    : buff(0xffff)
{
}

void SOCKET_IO_DATA::read_data()
{
    if (!sock) {
        return;
    }
    if (try_lock()) {
        while (true) {
            ssize_t count = read(sock, buff.data(), buff.size());

            if (count == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                break;
            }

            if (count < 0) {
                p_TP->runSheduled(1, &SOCKET_IO_DATA::close_connection, this);
                break;
            }

            read_buff.insert(read_buff.end(), buff.begin(), buff.begin() + count);

            if (read_complete()) {
                break;
            }
        }

        {
            timeout_prev_check = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count());
            in_use.store(false);
        }
    } else {
        p_TP->runSheduled(3, &SOCKET_IO_DATA::read_data, this);
    }
}

void SOCKET_IO_DATA::write_data()
{
    if (!sock) {
        return;
    }
    if (try_lock()) {
        while (!write_buff.empty()) {
            ssize_t count = write(sock, &write_buff[write_data_complete], write_buff.size() - write_data_complete);

            if (count == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                break;
            }

            if (count < 0) {
                p_TP->runSheduled(1, &SOCKET_IO_DATA::close_connection, this);
                break;
            }

            write_data_complete += count;

            if (write_buff.size() == write_data_complete) {
                write_buff.clear();
                write_data_complete = 0;
                write_complete();
                break;
            }
        }

        {
            timeout_prev_check = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count());
            in_use.store(false);
        }
    } else {
        p_TP->runSheduled(4, &SOCKET_IO_DATA::write_data, this);
    }
}

void SOCKET_IO_DATA::check_timeout(uint64_t _timeout)
{
    if (!sock) {
        return;
    }
    if (try_lock()) {
        uint64_t current_time_ms = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count());

        if (_timeout) {
            timeout = _timeout;
            timeout_prev_check = current_time_ms;
            p_TP->runSheduled(timeout, &SOCKET_IO_DATA::check_timeout, this, 0);
        } else {
            if (current_time_ms - timeout_prev_check > timeout) {
                p_TP->runSheduled(1, &SOCKET_IO_DATA::close_connection, this);
            } else {
                p_TP->runSheduled(timeout, &SOCKET_IO_DATA::check_timeout, this, 0);
            }
        }

        {
            in_use.store(false);
        }
    } else {
        p_TP->runSheduled(5, &SOCKET_IO_DATA::check_timeout, this, _timeout);
    }
}

bool SOCKET_IO_DATA::try_lock()
{
    bool false_state = false;
    return in_use.compare_exchange_strong(false_state, true);
}

void SOCKET_IO_DATA::close_connection()
{
    if (!sock) {
        return;
    }
    if (try_lock()) {
        socket_closed();

        shutdown(sock, SHUT_RDWR);
        close(sock);

        sock = 0;

        p_TP->runSheduled(timeout * 2, [this]() {
            delete this;
        });

        {
            in_use.store(false);
        }
    } else {
        p_TP->runSheduled(6, &SOCKET_IO_DATA::close_connection, this);
    }
}
