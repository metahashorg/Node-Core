#include <cstring>
#include <meta_log.hpp>
#include <thread_pool.hpp>

void ThreadPool::set_timer(uint64_t timeout_ms, ThreadPool::fn_type* task)
{
    uint64_t stop = timeout_ms + static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count());

    std::thread([this, stop, task]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            uint64_t time_ms = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count());

            if (time_ms >= stop) {
                runTask(task);
                return;
            }
        }
    })
        .detach();
}
