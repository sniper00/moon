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
    using timerid_t = uint32_t;

    namespace detail
    {
        inline int64_t millseconds()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }

        class timer_context
        {
        public:
			static constexpr int32_t times_mask = 0xFFFFFFF;

			enum flag
			{
				removed = 1 << 29,
				infinite = 1 << 30,
			};

            timer_context(int32_t duration,int32_t repeattimes)
                :duration_(duration)
                ,repeattimes_(repeattimes)   
            {
            }

            ~timer_context()
            {
            }

            int32_t duration()  const noexcept
            {
                return duration_;
            }

            bool repeattimes(int32_t value) noexcept
            {
                repeattimes_ = value;
                return (repeattimes_& times_mask)>0;
            }

            int32_t repeattimes()  const noexcept
            {
                return repeattimes_;
            }

			void set_flag(flag v) noexcept
			{
				repeattimes_ |= static_cast<int32_t>(v);
			}

			bool has_flag(flag v) const noexcept
			{
				return ((repeattimes_& static_cast<int32_t>(v)) != 0);
			}

			void clear_flag(flag v) noexcept
			{
				repeattimes_ &= ~static_cast<int32_t>(v);
			}
        private:
            int32_t	duration_;
            int32_t	repeattimes_;
        };

        template<typename TContainer, uint8_t TSize>
        class timer_wheel
        {
        public:
            timer_wheel()
                :m_head(0)
            {
            }

            TContainer& operator[](uint8_t pos)
            {
                assert(pos < TSize);
                return m_array[pos];
            }

            TContainer& front()
            {
                assert(m_head < TSize);
                return m_array[m_head];
            }

            void pop_front() noexcept
            {
                auto tmp = ++m_head;
                m_head = tmp % TSize;
            }

            bool round() const noexcept
            {
                return m_head == 0;
            }

            uint8_t size() const noexcept
            {
                return TSize;
            }

            size_t next_slot() const noexcept
            {
                return m_head;
            }
        private:
            TContainer 	m_array[TSize];
            uint32_t	m_head;
        };
    }

    template<class TChild>
    class base_timer
    {
        //every wheel size max 255
        static const uint8_t  WHEEL_SIZE = 255;

        static const int TIMERID_SHIT = 32;

        using timer_wheel_t = detail::timer_wheel<std::list<uint64_t>, WHEEL_SIZE>;
        using child_t = TChild;
    public:
        //precision ms
        static const int32_t PRECISION = 10;

        base_timer()
            : stop_(false)
            , tick_(0)
            , prew_tick_(0)
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

        void update()
        {
            auto now_tick = detail::millseconds();
            if (prew_tick_ == 0)
            {
                prew_tick_ = now_tick;
            }
            tick_ += (now_tick - prew_tick_);
            prew_tick_ = now_tick;

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
        uint64_t make_key(timerid_t id, uint32_t slots)
        {
            return ((static_cast<uint64_t>(id) << TIMERID_SHIT) | slots);
        }

        inline uint8_t get_slot(uint64_t  key, int which_queue)
        {
            return (key >> (which_queue * 8)) & 0xFF;
        }

        void insert_timer(int32_t duration, timerid_t id)
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
                timerid_t id = static_cast<timerid_t>(key >> TIMERID_SHIT);
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
        int64_t prew_tick_;
        std::vector <timer_wheel_t> wheels_;
    };


    class timer:public base_timer<timer>
    {
        static const int MAX_TIMER_NUM = (1 << 24) - 1;
        using timer_context_t = detail::timer_context;
    public:
        timerid_t repeat(int32_t duration, int32_t times)
        {
            if (!on_timer_ || !on_remove_)
            {
                return 0;
            }

            if (duration < PRECISION)
            {
                duration = PRECISION;
            }

			assert(times < detail::timer_context::times_mask);

            if (uuid_ == 0 || uuid_ == MAX_TIMER_NUM)
                uuid_ = 1;

            while (timers_.find(uuid_) != timers_.end())
            {
                ++uuid_;
            }

			if (times <= 0)
			{
				times = (0|detail::timer_context::infinite);
			}

            timerid_t id = uuid_;
            insert_timer(duration, id);
            timers_.emplace(id, timer_context_t{ duration,times });
            return id;
        }

        void remove(timerid_t timerid)
        {
            auto iter = timers_.find(timerid);
            if (iter != timers_.end())
            {
				iter->second.set_flag(detail::timer_context::removed);
                return;
            }
        }

        timerid_t create_timerid()
        {
            if (uuid_ == 0 || uuid_ == MAX_TIMER_NUM)
                uuid_ = 1;
            while (timers_.find(uuid_) != timers_.end())
            {
                ++uuid_;
            }
            return uuid_;
        }

        int32_t on_timer(timerid_t id)
        {
            auto iter = timers_.find(id);
            if (iter == timers_.end())
            {
                return 0;
            }

            auto&ctx = iter->second;
            if (!ctx.has_flag(detail::timer_context::removed))
            {
                on_timer_(id);
                if (ctx.has_flag(detail::timer_context::infinite) || ctx.repeattimes(ctx.repeattimes() - 1))
                {
                    return ctx.duration();
                }
            }
            on_remove_(id);
            timers_.erase(iter);
            return 0;
        }

        void set_remove_timer(const std::function<void(timerid_t)>& v)
        {
            on_remove_ = v;
        }

        void set_on_timer(const std::function<void(timerid_t)>& v)
        {
            on_timer_ = v;
        }
    private:
        uint32_t uuid_ = 0;
        std::unordered_map<uint32_t, timer_context_t> timers_;
        std::function<void(timerid_t)> on_remove_;
        std::function<void(timerid_t)> on_timer_;
    };

    using timer_t = timer;
}

