#pragma once
#include <cstdint>
#include <functional>
#include <cassert>
#include <list>
#include <array>
#include <unordered_map>


namespace moon
{
    using timer_id_t = uint32_t;

    namespace detail
    {
        template<typename TContainer, uint8_t Size>
        class timer_wheel
        {
            using container_t = TContainer;
        public:
            timer_wheel()
                :head_(0)
            {
            }

            container_t& operator[](size_t pos)
            {
                assert(pos < Size);
                return array_[pos];
            }

            container_t& front()
            {
                assert(head_ < Size);
                return array_[head_];
            }

            void tick() noexcept
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

            size_t now() const noexcept
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

        base_timer() = default;

        base_timer(const base_timer&) = delete;
        base_timer& operator=(const base_timer&) = delete;

        int64_t update(int64_t now)
        {
            if (prev_ == 0)
            {
                prev_ = now;
            }
            delta_ += (now - prev_);
            prev_ = now;

            auto delta = delta_;

            auto& wheels = wheels_;
            while (delta_ >= PRECISION)
            {
                delta_ -= PRECISION;
                if (stop_)
                    continue;
                for (size_t i = 0; i < wheels.size(); ++i)
                {
                    auto& wheel = wheels[i];
                    if (i + 1 == wheels.size())
                    {
                        break;
                    }
                    auto& next_wheel = wheels[i + 1];

                    wheel.tick();

                    if (wheel.round())
                    {
                        auto& timers = next_wheel.front();
                        while (!timers.empty())
                        {
                            auto key = timers.front();
                            timers.pop_front();
                            auto slot = get_slot(key, i);
                            wheel[slot].push_front(key);
                        }
                    }
                    else
                    {
                        break;
                    }
                }

                auto& timers = wheels[0].front();

                if (!timers.empty())
                {
                    expired(timers);
                }
            }
            return delta;
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

        uint8_t get_slot(uint64_t  key, size_t which_queue)
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
            slot_count = ((slot_count > 0) ? slot_count : 1);

            uint32_t slots = 0;

            for (size_t i = 0; i < wheels_.size(); ++i)
            {
                auto& wheel = wheels_[i];
                slot_count += wheel.now();
                if (slot_count < wheel.size())
                {
                    uint64_t key = make_key(id, slots);
                    wheel[slot_count].push_back(key);
                    break;
                }
                else
                {
                    auto slot = slot_count % (wheel.size());
                    slots |= (static_cast<uint32_t>(slot) << (i * 8));
                    slot_count /= wheel.size();
                    --slot_count;
                }
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
                child->on_timer(id);
            }
        }
    private:
        bool stop_ = false;
        int64_t delta_ = 0;
        int64_t prev_ = 0;
        std::array< timer_wheel_t, 4> wheels_;
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

        void on_timer(timer_id_t id)
        {
            auto iter = timers_.find(id);
            if (iter == timers_.end())
            {
                return;
            }

            auto&ctx = iter->second;
            if (!ctx.has_flag(timer_context::removed))
            {
                bool iscontinue = false;
                if (ctx.has_flag(timer_context::infinite) || ctx.times(ctx.times() - 1))
                {
                    insert_timer(ctx.duration(), id);
                    iscontinue = true;
                }
                ctx.expired(id);
                if (iscontinue)
                {
                    return;
                }
            }
            timers_.erase(iter);
            return;
        }

    private:
        uint32_t uuid_ = 0;
        std::unordered_map<uint32_t, context> timers_;
    };
}

