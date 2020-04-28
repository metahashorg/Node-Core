#include <thread_pool.hpp>

std::vector<boost::thread> thread_pool(boost::asio::io_context& io_context, uint64_t thread_count)
{
    std::vector<boost::thread> threadpool;
    boost::asio::io_context::work work(io_context);

    for (uint i = 0; i < thread_count; i++) {
        threadpool.push_back(
            boost::thread([&io_context]() {
                io_context.run();
            }));
    }

    return threadpool;
}