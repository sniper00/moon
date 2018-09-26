#pragma once
#include "common/timer.hpp"

namespace moon
{
    class lua_timer :public base_timer<lua_timer>
    {
        static constexpr int MAX_TIMER_NUM = (1 << 24) - 1;

        using timer_handler_t = std::function<void(timer_id_t)>;

        friend class base_timer<lua_timer>;
    public:
        timer_id_t repeat(int32_t duration, int32_t times)
        {
            if (!on_timer_ || !on_remove_)
            {
                return 0;
            }

            if (duration < PRECISION)
            {
                duration = PRECISION;
            }

            assert(times < timer_context::times_mask);

            if (uuid_ == 0 || uuid_ == MAX_TIMER_NUM)
                uuid_ = 1;

            while (timers_.find(uuid_) != timers_.end())
            {
                ++uuid_;
            }

            if (times <= 0)
            {
                times = (0 | timer_context::infinite);
            }

            timer_id_t id = uuid_;
            insert_timer(duration, id);
            timers_.emplace(id, timer_context{ duration,times });
            return id;
        }

        void remove(timer_id_t timerid)
        {
            auto iter = timers_.find(timerid);
            if (iter != timers_.end())
            {
                iter->second.set_flag(timer_context::removed);
                return;
            }
        }

        void set_remove_timer(const std::function<void(timer_id_t)>& v)
        {
            on_remove_ = v;
        }

        void set_on_timer(const std::function<void(timer_id_t)>& v)
        {
            on_timer_ = v;
        }

    private:
        timer_id_t create_timerid()
        {
            if (uuid_ == 0 || uuid_ == MAX_TIMER_NUM)
                uuid_ = 1;
            while (timers_.find(uuid_) != timers_.end())
            {
                ++uuid_;
            }
            return uuid_;
        }

        int32_t on_timer(timer_id_t id)
        {
            auto iter = timers_.find(id);
            if (iter == timers_.end())
            {
                return 0;
            }

            auto&ctx = iter->second;
            if (!ctx.has_flag(timer_context::removed))
            {
                on_timer_(id);
                if (ctx.has_flag(timer_context::infinite) || ctx.times(ctx.times() - 1))
                {
                    return ctx.duration();
                }
            }
            on_remove_(id);
            timers_.erase(iter);
            return 0;
        }
    private:
        uint32_t uuid_ = 0;
        std::unordered_map<uint32_t, timer_context> timers_;
        std::function<void(timer_id_t)> on_remove_;
        std::function<void(timer_id_t)> on_timer_;
    };
}

