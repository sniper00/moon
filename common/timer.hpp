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
            static constexpr int32_t  TIMER_INFINITE = -1;
            static constexpr int32_t  TIMER_REMOVED = -2;

            timer_context(int32_t duration)
                :repeattimes_(0)
                , duration_(duration)
                , id_(0)
            {
            }

            ~timer_context()
            {
            }

            void init(int32_t duration)
            {
                repeattimes_ = 0;
                duration_ = duration;
                id_ = 0;
            }

            void id(timerid_t value) noexcept
            {
                id_ = value;
            }

            timerid_t id() const noexcept
            {
                return id_;
            }

            int64_t duration()  const noexcept
            {
                return duration_;
            }

            int32_t repeattimes(int32_t value) noexcept
            {
                repeattimes_ = value;
                return repeattimes_;
            }

            int32_t repeattimes()  const noexcept
            {
                return repeattimes_;
            }

            void remove() noexcept
            {
                repeattimes_ = TIMER_REMOVED;
            }

            bool removed() const noexcept
            {
                return (repeattimes_ == TIMER_REMOVED);
            }

            bool forever() const noexcept
            {
                return (repeattimes_ == TIMER_INFINITE);
            }
        private:
            int32_t	repeattimes_;
            int32_t	duration_;
            timerid_t id_;
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

    class timer
    {
        using  timer_context_ptr = std::shared_ptr<detail::timer_context>;

        //every wheel size max 255
        static const uint8_t  WHEEL_SIZE = 255;
        //precision ms
        static const int32_t PRECISION = 10;

        static const int TIMERID_SHIT = 32;

        static const int MAX_TIMER_NUM = (1 << 16) - 1;

        using timer_wheel_t = detail::timer_wheel<std::list<uint64_t>, WHEEL_SIZE>;
    public:
        timer()
            : stop_(false)
            , uuid_(1)
            , tick_(0)
            , prew_tick_(0)
        {
            wheels_.emplace_back();
            wheels_.emplace_back();
            wheels_.emplace_back();
            wheels_.emplace_back();
        }

        timer(const timer&) = delete;
        timer& operator=(const timer&) = delete;

        ~timer()
        {
        }

        timerid_t  repeat(int32_t duration, int32_t times)
        {
            if (!on_timer_ || !on_remove_)
            {
                return 0;
            }

            if (duration < PRECISION)
            {
                duration = PRECISION;
            }

            auto pt = std::make_shared<detail::timer_context>(duration);
            pt->repeattimes(times);
            return add_new_timer(pt);
        }

        void			remove(timerid_t timerid)
        {
            auto iter = timers_.find(timerid);
            if (iter != timers_.end())
            {
                iter->second->remove();
                return;
            }
        }

        void			update()
        {
            auto nowTick = detail::millseconds();
            if (prew_tick_ == 0)
            {
                prew_tick_ = nowTick;
            }
            tick_ += (nowTick - prew_tick_);
            prew_tick_ = nowTick;

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

        void			stop_all_timer()
        {
            stop_ = true;
        }

        void			start_all_timer()
        {
            stop_ = false;
        }

        void set_on_timer(const std::function<void(timerid_t)>& v)
        {
            on_timer_ = v;
        }

        void set_remove_timer(const std::function<void(timerid_t)>& v)
        {
            on_remove_ = v;
        }

    private:
        // slots:      8bit(notuse) 8bit(wheel3_slot)  8bit(wheel2_slot)  8bit(wheel1_slot)  
        uint64_t make_key(timerid_t id, uint32_t slots)
        {
            return ((static_cast<uint64_t>(id) << TIMERID_SHIT) | slots);
        }

        inline uint8_t get_slot(uint64_t  key, int which_queue)
        {
            return (key >> (which_queue * 8)) & 0xFF;
        }

        timerid_t 	add_new_timer(timer_context_ptr t)
        {
            if (t->id() == 0)
            {
                if (uuid_ == 0 || uuid_ == MAX_TIMER_NUM)
                    uuid_ = 1;
                while (timers_.find(uuid_) != timers_.end())
                {
                    ++uuid_;
                }
                t->id(uuid_);
                timers_.emplace(t->id(), t);
            }
            add_timer(t);
            //new_timers_.push_back(t);
            return t->id();
        }

        void	add_timer(timer_context_ptr t)
        {
            auto diff = t->duration();
            auto offset = diff%PRECISION;
            if (offset > 0)
            {
                diff += PRECISION;
            }
            auto slot_count = diff / PRECISION;
            slot_count = (slot_count > 0) ? slot_count : 1;
            uint64_t key = 0;
            int i = 0;
            uint32_t slots = 0;
            for (auto it = wheels_.begin(); it != wheels_.end(); it++, i++)
            {
                auto& wheel = *it;
                slot_count += wheel.next_slot();
                uint8_t slot = (slot_count - 1) % (wheel.size());
                slot_count -= slot;
                slots |= (static_cast<uint32_t>(slot) << (i * 8));
                key = make_key(t->id(), slots);
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

        void	 expired(std::list<uint64_t>& expires)
        {
            while (!expires.empty())
            {
                auto key = expires.front();
                expires.pop_front();
                timerid_t id = static_cast<timerid_t>(key >> TIMERID_SHIT);
                auto iter = timers_.find(id);
                if (iter == timers_.end())
                    continue;

                auto&ctx = iter->second;

                if (!ctx->removed())
                {
                    on_timer_(id);

                    if (ctx->forever() || ctx->repeattimes(ctx->repeattimes() - 1))
                    {
                        add_timer(ctx);
                        continue;
                    }
                }
                on_remove_(id);
                timers_.erase(iter);
            }
        }
    private:
        bool stop_;
        uint32_t uuid_;
        int64_t tick_;
        int64_t prew_tick_;
        std::vector < timer_wheel_t> wheels_;
        std::unordered_map<uint32_t, timer_context_ptr> timers_;
        std::function<void(timerid_t)> on_timer_;
        std::function<void(timerid_t)> on_remove_;
    };
}

