#include <meta_client.h>
#include <meta_common.h>
//#include <meta_log.hpp>

namespace metahash::network {

ClientConnection::ClientConnection(boost::asio::io_context& io_context, boost::asio::ip::basic_resolver<boost::asio::ip::tcp>::results_type& endpoints, moodycamel::ConcurrentQueue<Task*>& tasks, std::string mh_endpoint_addr)
    : mh_endpoint_addr(std::move(mh_endpoint_addr))
    , io_context(io_context)
    , serial_execution(io_context)
    , endpoints(endpoints)
    , socket(new boost::asio::ip::tcp::socket(serial_execution))
    , tasks(tasks)
    , timer(serial_execution, boost::posix_time::milliseconds(10))
{
}

void ClientConnection::try_connect()
{
    boost::asio::async_connect(*socket, endpoints, [this](const boost::system::error_code& err, const boost::asio::ip::basic_endpoint<boost::asio::ip::tcp>&) {
        if (!err) {
            connected = true;
            check_tasks();
        } else {
            timer = boost::asio::deadline_timer(serial_execution, boost::posix_time::milliseconds(100));
            timer.async_wait([this](const boost::system::error_code&) {
                try_connect();
            });
        }
    });
}

void ClientConnection::check_tasks()
{
    if (tasks.try_dequeue(p_task) && p_task) {
        boost::asio::async_write(*socket, boost::asio::buffer(p_task->write_buff), [this](const boost::system::error_code& err, std::size_t) {
            if (!err) {
                read();
            } else {
                //DEBUG_COUT("error");
                //DEBUG_COUT(err.message());

                reset();
            }
        });
    } else {
        timer = boost::asio::deadline_timer(serial_execution, boost::posix_time::milliseconds(10));
        timer.async_wait([this](const boost::system::error_code&) {
            check_tasks();
        });
    }
}

void ClientConnection::read()
{
    socket->async_read_some(boost::asio::buffer(buffer), [this](const boost::system::error_code& err, std::size_t bytes_transferred) {
        if (!err) {
            auto result = static_cast<statics::parse_state>(response.parse(buffer.data(), bytes_transferred, mh_endpoint_addr));

            switch (result) {
            case statics::SUCCESS: {
                std::vector<char> buff;
                buff.insert(buff.end(), response.message.begin(), response.message.end());

                p_task->callback(buff);

                response = {};
                delete p_task;
                p_task = nullptr;

                check_tasks();
            } break;
            case statics::INCOMPLETE: {
                read();
            } break;
            case statics::WRONG_MAGIC_NUMBER:
            case statics::UNKNOWN_SENDER_METAHASH_ADDRESS:
            case statics::INVALID_SIGN: {
                //DEBUG_COUT("ERROR");
                p_task->callback(std::vector<char>());
                reset();
            } break;
            }

        } else {
            //DEBUG_COUT("error");
            //DEBUG_COUT(err.message());

            p_task->callback(std::vector<char>());
            reset();
        }
    });
}

void ClientConnection::reset()
{
    connected = false;
    socket.reset(new boost::asio::ip::tcp::socket(serial_execution));
    try_connect();
}

bool ClientConnection::online()
{
    return connected;
}

void ClientConnection::execute_callback(std::vector<char>& data)
{
    auto task = p_task;
    io_context.post([task, data] {
        task->callback(data);
        delete task;
    });

    response = {};
    p_task = nullptr;
}

}
