#pragma once
#include "common/timer.hpp"

namespace moon
{
    class worker_timer :public base_timer<worker_timer>
    {
        static constexpr int MAX_TIMER_NUM = (1 << 24) - 1;

        using timer_handler_t = std::function<void(timer_id_t, uint32_t, bool)>;

        friend class base_timer<worker_timer>;

        struct worker_timer_context:public timer_context
        {
            worker_timer_context(int32_t duration, int32_t times, uint32_t sid)
                :timer_context(duration, times)
                , serviceid(sid)
            {

            }
            uint32_t serviceid;
        };
    public:
        timer_id_t repeat(int32_t duration, int32_t times, uint32_t serviceid)
        {
            if (!on_timer_)
            {
                return 0;
            }

            if (duration < PRECISION)
            {
                duration = PRECISION;
            }

            assert(times < worker_timer_context::times_mask);

            if (uuid_ == 0 || uuid_ == MAX_TIMER_NUM)
                uuid_ = 1;

            while (timers_.find(uuid_) != timers_.end())
            {
                ++uuid_;
            }

            if (times <= 0)
            {
                times = (0 | worker_timer_context::infinite);
            }

            timer_id_t id = uuid_;
            insert_timer(duration, id);
            timers_.emplace(id, worker_timer_context{ duration,times,serviceid });
            return id;
        }

        void remove(timer_id_t timerid)
        {
            auto iter = timers_.find(timerid);
            if (iter != timers_.end())
            {
                iter->second.set_flag(worker_timer_context::removed);
                return;
            }
        }

        void set_on_timer(const timer_handler_t& v)
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
            if (!ctx.has_flag(worker_timer_context::removed))
            {
                on_timer_(id, ctx.serviceid, false);
                if (ctx.has_flag(worker_timer_context::infinite) || ctx.times(ctx.times() - 1))
                {
                    return ctx.duration();
                }
            }
            on_timer_(id, ctx.serviceid, true);
            timers_.erase(iter);
            return 0;
        }
    private:
        uint32_t uuid_ = 0;
        std::unordered_map<uint32_t, worker_timer_context> timers_;
        timer_handler_t on_timer_;
    };
}

