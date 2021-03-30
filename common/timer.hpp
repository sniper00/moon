#pragma once
#include <cstdint>
#include <functional>
#include <cassert>
#include <map>

namespace moon
{
    template<typename ExpirePolicy>
    class base_timer
    {
        using expire_policy_type = ExpirePolicy;
    public:
        base_timer() = default;

        base_timer(const base_timer&) = delete;
        base_timer& operator=(const base_timer&) = delete;

        void update(int64_t now)
        {
            if (stop_)
            {
                return;
            }

            do
            {
                expire_policy_type v;
                {
                    std::lock_guard lock{ lock_ };
                    auto iter = timers_.begin();
                    if (iter == timers_.end())
                    {
                        break;
                    }

                    auto t = iter->first;

                    if (t > now)
                    {
                        break;
                    }
                    v = std::move(iter->second);
                    timers_.erase(iter);
                }
                v();
            } while (true);
            return;
        }

        void stop_all_timer()
        {
            stop_ = true;
        }

        void start_all_timer()
        {
            stop_ = false;
        }

        template<typename... Args>
        void add(time_t expiretime, Args&&... args)
        {
            std::lock_guard lock{ lock_ };
            timers_.emplace(expiretime, expire_policy_type{std::forward<Args>(args)... });
        }

        size_t size() const
        {
            return timers_.size();
        }
    private:
        bool stop_ = false;
        std::multimap<int64_t, expire_policy_type> timers_;
        std::mutex lock_;
    };

    class default_expire_policy
    {
    public:
        using handler_type = std::function<void()>;

        default_expire_policy(handler_type handler)
            :handler_(std::move(handler))
        {
        }

        void  operator()()
        {
            handler_();
        }
    private:
        handler_type handler_;
    };

    using timer = base_timer<default_expire_policy>;
}


