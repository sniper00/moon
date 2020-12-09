#pragma once
#include <cstdint>
#include <functional>
#include <cassert>
#include <map>
#include <unordered_map>

namespace moon
{
    using timer_t = uint32_t;

    template<typename ExpirePolicy>
    class base_timer
    {
        using expire_policy_type = ExpirePolicy;
        struct context
        {
        public:
            template<typename ...Args>
            context(int32_t times, int64_t interval, Args&&... args)
                :times_(times), interval_(interval), policy_(std::forward<Args>(args)...) {}

            bool continued() noexcept { return (times_ < 0) || ((--times_) > 0); }

            int32_t	times_;
            int64_t interval_;
            expire_policy_type policy_;
        };
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
                auto iter = tickers_.begin();
                if (iter == tickers_.end())
                {
                    break;
                }

                auto t = iter->first;

                if (t > now)
                {
                    break;
                }

                auto id = iter->second;
                tickers_.erase(iter);
                expired(t, id);
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
        timer_t repeat(int64_t now, int64_t interval, int32_t times, Args&&... args)
        {
            if (interval <= 0)
            {
                interval = 1;
            }

            timer_t id = create_timerid();
            tickers_.emplace(now + interval, id);
            timers_.emplace(id, context{ times, interval, std::forward<Args>(args)... });
            return id;
        }

        void remove(timer_t timerid)
        {
            if (auto iter = timers_.find(timerid); iter != timers_.end())
            {
                iter->second.interval_ = 0;
            }
        }

        size_t size() const
        {
            return timers_.size();
        }

    private:
        timer_t create_timerid()
        {
            do
            {
                ++uuid_;
                if (uuid_ == 0 || uuid_ == std::numeric_limits<uint32_t>::max())
                    uuid_ = 1;
            } while (timers_.find(uuid_) != timers_.end());
            return uuid_;
        }

        void expired(int64_t now, timer_t id)
        {
            if (auto iter = timers_.find(id); iter != timers_.end())
            {
                auto& ctx = iter->second;
                if (ctx.interval_ == 0)//removed
                {
                    return;
                }
                bool continued = ctx.continued();
                if (continued)
                {
                    tickers_.emplace(now + ctx.interval_, id);
                }
                ctx.policy_(id, !continued);
                if (continued)
                {
                    return;
                }
                timers_.erase(id);
            }
        }
    private:
        bool stop_ = false;
        uint32_t uuid_ = 0;
        std::multimap<int64_t, uint32_t> tickers_;
        std::unordered_map<uint32_t, context> timers_;
    };

    class default_expire_policy
    {
    public:
        using handler_type = std::function<void(timer_t)>;

        default_expire_policy(handler_type handler)
            :handler_(std::move(handler))
        {
        }

        void  operator()(timer_t id, bool last)
        {
            (void)last;
            assert(handler_);
            handler_(id);
        }
    private:
        handler_type handler_;
    };

    using timer = base_timer<default_expire_policy>;
}


