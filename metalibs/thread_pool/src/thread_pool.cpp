#include <thread_pool.hpp>

std::tuple<std::vector<boost::thread>, boost::asio::io_context::work> thread_pool(boost::asio::io_context& io_context, uint64_t thread_count)
{
    std::vector<boost::thread> threadpool;
    boost::asio::io_context::work work(io_context);

    for (uint i = 0; i < thread_count; i++) {
        threadpool.emplace_back([&io_context]() {
                io_context.run();
            });
    }

    return { std::move(threadpool), std::move(work) };
}