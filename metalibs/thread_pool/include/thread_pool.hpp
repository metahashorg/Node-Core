#ifndef ThreadPool_hpp
#define ThreadPool_hpp

#include <atomic>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <functional>
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

        functionQueue.enqueue(task);
    }

    template <class _FN, class... _ARGS>
    void runSheduled(uint64_t timeout_ms, _FN _fn, _ARGS... _args)
    {
        uint64_t stop = timeout_ms + static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count());
        auto* task = new fn_type(std::bind(_fn, _args...));

        scheduledQueue.enqueue({ stop, task });
    }

    void runTask(fn_type* task)
    {
        functionQueue.enqueue(task);
    }

private:
    const bool affinity;
    const size_t thread_count;

    std::atomic<bool> goon = true;
    moodycamel::ConcurrentQueue<fn_type*> functionQueue;
    std::vector<std::thread> workers;

    moodycamel::ConcurrentQueue<std::pair<uint64_t, fn_type*>> scheduledQueue;
    std::deque<std::pair<uint64_t, fn_type*>> scheduled_tasks;
    uint64_t last_scheduler_check = 0;
    std::atomic<bool> scheduler_locked = false;

    void worker_thread(size_t thread_id)
    {
        if (affinity) {
            set_affinity(thread_id);
        }

        moodycamel::ConsumerToken function_consumer(functionQueue);
        std::pair<uint64_t, fn_type*> tasks_got[1000];

        while (goon.load()) {
            uint64_t time_ms = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count());

            if (time_ms > last_scheduler_check) {
                bool should_be_locked = false;
                bool would_be_locked = true;
                if (scheduler_locked.compare_exchange_strong(should_be_locked, would_be_locked)) {
                    last_scheduler_check = time_ms;
                    std::deque<fn_type*> ready_tasks;

                    {
                        uint64_t got_;
                        got_ = scheduledQueue.try_dequeue_bulk(&tasks_got[0], 1000);

                        scheduled_tasks.insert(scheduled_tasks.end(), &tasks_got[0], &tasks_got[0] + got_);

                        std::sort(scheduled_tasks.begin(), scheduled_tasks.end(), [](std::pair<uint64_t, fn_type*>& lh, std::pair<uint64_t, fn_type*>& rh) {
                            return lh.first < rh.first;
                        });

                        for (auto it = scheduled_tasks.begin(); it != scheduled_tasks.end();) {
                            if (time_ms < it->first) {
                                break;
                            }

                            ready_tasks.push_back(it->second);
                            scheduled_tasks.pop_front();
                            it = scheduled_tasks.begin();
                        }
                    }

                    functionQueue.enqueue_bulk(ready_tasks.begin(), ready_tasks.size());
                    scheduler_locked.store(false);
                }
            }

            if (fn_type * fn; functionQueue.try_dequeue(function_consumer, fn)) {
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

#endif /* ThreadPool_h */
