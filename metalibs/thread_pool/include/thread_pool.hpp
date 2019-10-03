#ifndef ThreadPool_hpp
#define ThreadPool_hpp

#include <atomic>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <shared_mutex>
// #include <signal.h>
#include <thread>
// #include <time.h>

#include <pthread.h>

#include "concurrentqueue.h"

class ThreadPool;

struct HandlerData {
    ThreadPool* TP;
    std::function<void()>* func;
    uint64_t timeout;
    uint64_t start;
};

class ThreadPool {
public:
    using fn_type = std::function<void()>;

    ThreadPool(size_t threads, bool _affinity)
        : affinity(_affinity)
        , thread_count(threads)
    {
        for (size_t i = 0; i < thread_count; i++) {
            workers.emplace_back(&ThreadPool::worker_thread, this, i);
        }
    }

    ThreadPool()
        : ThreadPool(std::thread::hardware_concurrency(), true)
    {
    }

    ThreadPool(size_t threads)
        : ThreadPool(threads, true)
    {
    }

    ~ThreadPool()
    {
        goon.store(false);
        for (auto& worker : workers) {
            worker.join();
        }
    }

    size_t get_thread_count()
    {
        return thread_count;
    }

    template <class _FN, class... _ARGS>
    void runAsync(_FN _fn, _ARGS... _args)
    {
        auto* task = new fn_type(std::bind(_fn, _args...));

        FunctionQueue.enqueue(task);
    }

    template <class _FN, class... _ARGS>
    void runSheduled(uint64_t timeout_ms, _FN _fn, _ARGS... _args)
    {
        auto* task = new fn_type(std::bind(_fn, _args...));

        set_timer(timeout_ms, task);
    }

    void runTask(fn_type* task)
    {
        FunctionQueue.enqueue(task);
    }

private:
    const bool affinity;
    const size_t thread_count;

    std::atomic<bool> goon = true;
    moodycamel::ConcurrentQueue<fn_type*> FunctionQueue;
    std::vector<std::thread> workers;

    void set_timer(uint64_t timeout_ms, fn_type* task);

    void worker_thread(size_t thread_id)
    {
        if (affinity) {
            set_affinity(thread_id);
        }

        moodycamel::ConsumerToken function_consumer(FunctionQueue);

        while (goon.load()) {
            if (fn_type * fn; FunctionQueue.try_dequeue(function_consumer, fn)) {
                (*fn)();
                delete fn;
                continue;
            }

            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }

    void set_affinity(size_t thread_id)
    {
        cpu_set_t cpuset;

        CPU_ZERO(&cpuset);
        CPU_SET(thread_id % std::thread::hardware_concurrency(), &cpuset);

        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    }
};

class EventPool {
    int epoll_io_fd = 0;
};

#endif /* ThreadPool_h */
