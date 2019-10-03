#ifndef THREADS_SPINLOCK_HPP_
#define THREADS_SPINLOCK_HPP_

#include <atomic>
#include <memory>

namespace mh::threads {

class spinlock final
{
private:
    typedef enum
    {
        Locked,
        Unlocked
    } LockState;

    std::atomic<LockState> state_;

public:
    spinlock() : state_(Unlocked)
    {}

    void lock()
    {
        while (state_.exchange(Locked, std::memory_order_acquire) == Locked) {
            /* busy-wait */
        }
    }

    void unlock()
    {
        state_.store(Unlocked, std::memory_order_release);
    }

    bool try_lock()
    {
        return !(state_.exchange(Locked, std::memory_order_acquire) == Locked);
    }
};

} // namespace mh::threads


#endif
