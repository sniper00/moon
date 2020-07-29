#pragma once
#include <cstdint>
#include <functional>
#include <cassert>
#include <list>
#include <array>
#include <unordered_map>

namespace moon
{
    using timer_t = uint32_t;

    namespace detail
    {
        template<typename Container, size_t Size>
        class timer_wheel
        {
            using container_t = Container;
        public:
            timer_wheel()
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

            size_t size() const noexcept
            {
                return Size;
            }

            size_t now() const noexcept
            {
                return (head_);
            }
        private:
            size_t head_ = 0;
            container_t array_[Size];
        };
    }

    template<typename ExpirePolicy>
    class base_timer
    {
        using expire_policy_type = ExpirePolicy;
        class context
        {
        public:
            template<typename ...Args>
            context(uint32_t slotcount, int32_t times, Args&&... args)
                :slotcount_(slotcount), times_(times), policy(std::forward<Args>(args)...) {}

            void slotcount(uint32_t v) noexcept { slotcount_ = v; }

            uint32_t slotcount()  const noexcept { return slotcount_; }

            bool continued() noexcept { return (times_ < 0) || ((--times_) > 0); }
        private:
            uint32_t	slotcount_;
            int32_t	times_;
        public:
            expire_policy_type policy;
        };
    public:
        static constexpr int TIMERID_SHIFT = 32;
        //precision ms
        static constexpr int PRECISION = 10;

        //each time wheel size 255
        using timer_wheel_t = detail::timer_wheel<std::list<uint64_t>, 255>;

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

                    wheel.tick();

                    if (i + 1 == wheels.size())
                    {
                        break;
                    }
                    auto& next_wheel = wheels[i + 1];

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

        template<typename... Args>
        timer_t repeat(int64_t duration, int32_t times, Args&&... args)
        {
            if (duration < PRECISION)
            {
                duration = PRECISION;
            }

            if ((duration % PRECISION) != 0)
            {
                duration += PRECISION;
            }

            size_t slot_count = duration / PRECISION;
            if (slot_count > std::numeric_limits<uint32_t>::max())
            {
                return 0;
            }

            timer_t id = create_timerid();

            insert_timer(slot_count, id);
            timers_.emplace(id, context{ static_cast<uint32_t>(slot_count), times, std::forward<Args>(args)... });
            return id;
        }

        void remove(timer_t timerid)
        {
            if (auto iter = timers_.find(timerid); iter != timers_.end())
            {
                iter->second.slotcount(0);
            }
        }

        size_t size()
        {
            return timers_.size();
        }

    private:
        // slots:      8bit(notuse) 8bit(wheel3_slot)  8bit(wheel2_slot)  8bit(wheel1_slot)  
        uint64_t make_key(timer_t id, uint32_t slots)
        {
            return ((static_cast<uint64_t>(id) << TIMERID_SHIFT) | slots);
        }

        uint8_t get_slot(uint64_t  key, size_t which_queue)
        {
            return (key >> (which_queue * 8)) & 0xFF;
        }

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

        void insert_timer(size_t slot_count, timer_t id)
        {
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
                timer_t id = static_cast<timer_t>(key >> TIMERID_SHIFT);
                if (auto iter = timers_.find(id); iter == timers_.end())
                {
                    continue;
                }
                else
                {
                    auto& ctx = iter->second;
                    if (ctx.slotcount() > 0)//not removed
                    {
                        bool continued = ctx.continued();
                        if (continued)
                        {
                            insert_timer(ctx.slotcount(), id);
                        }
                        ctx.policy(id, !continued);
                        if (continued)
                        {
                            continue;
                        }
                    }
                    timers_.erase(iter);
                }
            }
        }
    private:
        bool stop_ = false;
        int64_t delta_ = 0;
        int64_t prev_ = 0;
        uint32_t uuid_ = 0;
        std::array< timer_wheel_t, 4> wheels_;
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

