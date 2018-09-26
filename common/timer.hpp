/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include <cstdint>
#include <memory>
#include <functional>
#include <cassert>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <list>

namespace moon
{
    using timer_id_t = uint32_t;

    namespace detail
    {
        inline int64_t millseconds()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }

        template<typename TContainer, uint8_t Size>
        class timer_wheel
        {
            using container_t = TContainer;
        public:
            timer_wheel()
                :head_(0)
            {
            }

            container_t& operator[](uint8_t pos)
            {
                assert(pos < Size);
                return array_[pos];
            }

            container_t& front()
            {
                assert(head_ < Size);
                return array_[head_];
            }

            void pop_front() noexcept
            {
                auto tmp = ++head_;
                head_ = tmp % Size;
            }

            bool round() const noexcept
            {
                return head_ == 0;
            }

            uint8_t size() const noexcept
            {
                return Size;
            }

            size_t next_slot() const noexcept
            {
                return head_;
            }
        private:
            container_t 	array_[Size];
            uint32_t	head_;
        };
    }

    template<class TChild>
    class base_timer
    {
        //every wheel size max 255
        static constexpr int  WHEEL_SIZE = 255;

        static constexpr int TIMERID_SHIT = 32;

        using timer_wheel_t = detail::timer_wheel<std::list<uint64_t>, WHEEL_SIZE>;
        using child_t = TChild;
    public:
        //precision ms
        static const int32_t PRECISION = 10;

        base_timer()
            : stop_(false)
            , tick_(0)
            , previous_tick_(0)
        {
            wheels_.emplace_back();
            wheels_.emplace_back();
            wheels_.emplace_back();
            wheels_.emplace_back();
        }

        base_timer(const base_timer&) = delete;
        base_timer& operator=(const base_timer&) = delete;

        ~base_timer()
        {
        }

        int64_t update()
        {
            auto now_tick = detail::millseconds();
            if (previous_tick_ == 0)
            {
                previous_tick_ = now_tick;
            }
            tick_ += (now_tick - previous_tick_);
            previous_tick_ = now_tick;

            auto old_tick = tick_;

            auto& wheels = wheels_;
            while (tick_ >= PRECISION)
            {
                tick_ -= PRECISION;
                if (stop_)
                    continue;
                {
                    auto& timers = wheels[0].front();
                    wheels[0].pop_front();
                    if (!timers.empty())
                    {
                        expired(timers);
                    }
                }

                int i = 0;
                for (auto wheel = wheels.begin(); wheel != wheels.end(); ++wheel, ++i)
                {
                    auto next_wheel = wheel;
                    if (wheels.end() == (++next_wheel))
                        break;

                    if (wheel->round())
                    {
                        auto& timers = next_wheel->front();
                        while (!timers.empty())
                        {
                            auto key = timers.front();
                            timers.pop_front();
                            auto slot = get_slot(key, i);
                            (*wheel)[slot].push_front(key);
                            //printf("update timer id %u add to wheel [%d] slot [%d]\r\n", (key>>32), i+1, slot);
                        }
                        next_wheel->pop_front();
                    }
                    else
                    {
                        break;
                    }
                }
            }
            return old_tick;
        }

        void stop_all_timer()
        {
            stop_ = true;
        }

        void start_all_timer()
        {
            stop_ = false;
        }

    protected:
        // slots:      8bit(notuse) 8bit(wheel3_slot)  8bit(wheel2_slot)  8bit(wheel1_slot)  
        uint64_t make_key(timer_id_t id, uint32_t slots)
        {
            return ((static_cast<uint64_t>(id) << TIMERID_SHIT) | slots);
        }

        inline uint8_t get_slot(uint64_t  key, int which_queue)
        {
            return (key >> (which_queue * 8)) & 0xFF;
        }

        void insert_timer(int32_t duration, timer_id_t id)
        {
            auto diff = duration;
            auto offset = diff % PRECISION;
            if (offset > 0)
            {
                diff += PRECISION;
            }
            size_t slot_count = diff / PRECISION;
            slot_count = (slot_count > 0) ? slot_count : 1;
            uint64_t key = 0;
            int i = 0;
            uint32_t slots = 0;
            for (auto it = wheels_.begin(); it != wheels_.end(); ++it, ++i)
            {
                auto& wheel = *it;
                slot_count += wheel.next_slot();
                uint8_t slot = (slot_count - 1) % (wheel.size());
                slot_count -= slot;
                slots |= (static_cast<uint32_t>(slot) << (i * 8));
                key = make_key(id, slots);
                //printf("process timer id %u wheel[%d] slot[%d]\r\n", t->id(), i+1, slot);
                if (slot_count < wheel.size())
                {
                    //printf("timer id %u add to wheel [%d] slot [%d]\r\n",t->id(),  i + 1, slot);
                    wheel[slot].push_back(key);
                    break;
                }
                slot_count /= wheel.size();
            }
        }

        void expired(std::list<uint64_t>& expires)
        {
            while (!expires.empty())
            {
                auto key = expires.front();
                expires.pop_front();
                timer_id_t id = static_cast<timer_id_t>(key >> TIMERID_SHIT);
                child_t* child = static_cast<child_t*>(this);
                int32_t duration = child->on_timer(id);
                if (duration != 0)
                {
                    insert_timer(duration, id);
                }
            }
        }
    private:
        bool stop_;
        int64_t tick_;
        int64_t previous_tick_;
        std::vector <timer_wheel_t> wheels_;
    };

    class timer_context
    {
    public:
        static constexpr int32_t times_mask = 0xFFFFFFF;

        enum flag
        {
            removed = 1 << 29,
            infinite = 1 << 30,
        };

        timer_context(int32_t duration, int32_t times)
            :duration_(duration)
            , times_(times)
        {
        }

        ~timer_context()
        {
        }

        int32_t duration()  const noexcept
        {
            return duration_;
        }

        bool times(int32_t value) noexcept
        {
            times_ = value;
            return (times_& times_mask) > 0;
        }

        int32_t times()  const noexcept
        {
            return times_;
        }

        void set_flag(flag v) noexcept
        {
            times_ |= static_cast<int32_t>(v);
        }

        bool has_flag(flag v) const noexcept
        {
            return ((times_& static_cast<int32_t>(v)) != 0);
        }

        void clear_flag(flag v) noexcept
        {
            times_ &= ~static_cast<int32_t>(v);
        }
    private:
        int32_t	duration_;
        int32_t	times_;
    };

    class timer :public base_timer<timer>
    {
        static constexpr int MAX_TIMER_NUM = (1 << 24) - 1;

        using timer_handler_t = std::function<void(timer_id_t)>;

        friend class base_timer<timer>;

        class context :public timer_context
        {
        public:
            context(int32_t duration, int32_t times, timer_handler_t handler)
                :timer_context(duration, times)
                , handler_(std::forward<timer_handler_t>(handler))
            {
            }

            ~context()
            {
            }

            void  expired(timer_id_t id)
            {
                assert(nullptr != handler_);
                handler_(id);
            }
        private:
            timer_handler_t handler_;
        };

    public:
        timer_id_t repeat(int32_t duration, int32_t times, timer_handler_t hander)
        {
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
            timers_.emplace(id, context{ duration,times, hander });
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
                ctx.expired(id);
                if (ctx.has_flag(timer_context::infinite) || ctx.times(ctx.times() - 1))
                {
                    return ctx.duration();
                }
            }
            timers_.erase(iter);
            return 0;
        }

    private:
        uint32_t uuid_ = 0;
        std::unordered_map<uint32_t, context> timers_;
    };
}

