#ifndef META_POOL_HPP
#define META_POOL_HPP

#include <boost/asio.hpp>

#include <thread>
#include <vector>

namespace metahash::pool {

std::tuple<std::vector<std::thread>, boost::asio::io_context::work> thread_pool(boost::asio::io_context& io_context, uint64_t thread_count);

}

#endif /* META_POOL_HPP */
