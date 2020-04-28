#ifndef ThreadPool_hpp
#define ThreadPool_hpp

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>

#include <utility>
#include <vector>

std::vector<boost::thread> thread_pool(boost::asio::io_context& io_context, uint64_t thread_count);

#endif /* ThreadPool_h */
