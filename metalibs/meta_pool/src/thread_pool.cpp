#include <meta_pool.hpp>

#include <utility>

namespace metahash::pool {

std::tuple<std::vector<std::thread>, boost::asio::io_context::work> thread_pool(boost::asio::io_context& io_context, uint64_t thread_count)
{
    std::vector<std::thread> threadpool;
    boost::asio::io_context::work work(io_context);

    for (uint i = 0; i < thread_count; i++) {
        threadpool.emplace_back([&io_context]() {
            io_context.run();
        });
    }

    return { std::move(threadpool), std::move(work) };
}

}