#pragma once
#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <unordered_set>

namespace moon {
template<typename ExpirePolicy>
class base_timer {
    using expire_policy_type = ExpirePolicy;

public:
    base_timer() = default;

    base_timer(const base_timer&) = delete;
    base_timer& operator=(const base_timer&) = delete;

    void update(int64_t now) {
        if (stop_) {
            return;
        }

        do {
            std::unique_lock lock { lock_ };
            if (auto iter = timers_.begin(); iter == timers_.end() || iter->first > now) {
                break;
            } else {
                auto nh = timers_.extract(iter);
                lock.unlock();
                nh.mapped()();
            }
        } while (true);
        return;
    }

    void pause() {
        stop_ = true;
    }

    void resume() {
        stop_ = false;
    }

    template<typename... Args>
    void add(time_t expiretime, Args&&... args) {
        std::lock_guard lock { lock_ };
        timers_.emplace(expiretime, expire_policy_type { std::forward<Args>(args)... });
    }

    size_t size() const {
        return timers_.size();
    }

private:
    bool stop_ = false;
    std::mutex lock_;
    std::multimap<int64_t, expire_policy_type> timers_;
};

class default_expire_policy {
public:
    using handler_type = std::function<void()>;

    default_expire_policy(uint32_t, handler_type handler): handler_(std::move(handler)) {}

    void operator()() {
        handler_();
    }

private:
    handler_type handler_;
};

using timer = base_timer<default_expire_policy>;
} // namespace moon
