#pragma once
#include <atomic>
#include <thread>

//https://probablydance.com/2019/12/30/measuring-mutexes-spinlocks-and-how-bad-the-linux-scheduler-really-is/
//https://rigtorp.se/spinlock/

namespace moon {
class spin_lock {
    std::atomic<bool> lock_ = { false };

public:
    bool try_lock() noexcept {
        // First do a relaxed load to check if lock is free in order to prevent
        // unnecessary cache misses if someone does while(!try_lock())
        return !lock_.load(std::memory_order_relaxed)
            && !lock_.exchange(true, std::memory_order_acquire);
    }

    void lock() noexcept {
        for (;;) {
            // Optimistically assume the lock is free on the first try
            if (!lock_.exchange(true, std::memory_order_acquire)) {
                return;
            }
            // Wait for lock to be released without generating cache misses
            uint_fast32_t counter = 0;
            while (lock_.load(std::memory_order_relaxed)) {
                if (++counter > 1000)
                    std::this_thread::yield();
            }
        }
    }

    void unlock() noexcept {
        lock_.store(false, std::memory_order_release);
    }
};
} // namespace moon
