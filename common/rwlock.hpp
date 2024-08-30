#pragma once
#include <atomic>
#include <cstdint>

/* RWSpinLock */
namespace moon {
class rwlock {
    enum : int32_t { READER = 2, WRITER = 1 };

public:
    rwlock(): bits_(0) {}

    rwlock(rwlock const&) = delete;
    rwlock& operator=(rwlock const&) = delete;

    bool try_lock_shared() noexcept {
        // fetch_add is considerably (100%) faster than compare_exchange,
        // so here we are optimizing for the common (lock success) case.
        int32_t value = bits_.fetch_add(READER, std::memory_order_acquire);
        if ((value & WRITER)) {
            bits_.fetch_add(-READER, std::memory_order_release);
            return false;
        }
        return true;
    }

    void lock_shared() noexcept {
        uint_fast32_t count = 0;
        while (!(try_lock_shared())) {
            if (++count > 1000) {
                std::this_thread::yield();
            }
        }
    }

    // Attempt to acquire writer permission. Return false if we didn't get it.
    bool try_lock() noexcept {
        int32_t expect = 0;
        return bits_.compare_exchange_strong(expect, WRITER, std::memory_order_acq_rel);
    }

    void lock() noexcept {
        uint_fast32_t count = 0;
        while (!try_lock()) {
            if (++count > 1000) {
                std::this_thread::yield();
            }
        }
    }

    void unlock() noexcept {
        static_assert(READER > WRITER, "wrong bits!");
        bits_.fetch_and(~WRITER, std::memory_order_release);
    }

    void unlock_shared() noexcept {
        bits_.fetch_add(-READER, std::memory_order_release);
    }

private:
    std::atomic<int32_t> bits_;
};
} // namespace moon
