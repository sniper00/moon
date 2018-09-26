#pragma once
#include <atomic>
#include <thread>
#include "noncopyable.hpp"

namespace moon
{
    class spin_lock : public moon::noncopyable
    {
        std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

    public:
        bool try_lock()
        {
            return !flag_.test_and_set(std::memory_order_acquire);
        }

        void lock()
        {
            int counter = 0;
            while (!try_lock())
            {
                if (++counter > 1000)
                    std::this_thread::yield();
            }
        }

        void unlock()
        {
            flag_.clear(std::memory_order_release);
        }
    };
}
