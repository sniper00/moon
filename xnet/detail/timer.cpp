/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#include "timer.h"
#include "timer_wheel.hpp"
#include "timer_context.hpp"

using namespace moon;

struct timer::timer_pool_imp
{
	std::vector < timer_wheel<std::vector<uint64_t>, WHEEL_SIZE>> wheels_;
	std::unordered_map<uint64_t, timer_context_ptr> timers_;
	std::vector<timer_context_ptr> new_timers_;
};

timer::timer()
:imp_(nullptr),m_tick(0), m_prewTick(0),m_inc(1), m_Stop(false)
{
	imp_ = new timer_pool_imp;

	imp_->wheels_.emplace_back(timer_wheel<std::vector<uint64_t>, WHEEL_SIZE>());
	imp_->wheels_.emplace_back(timer_wheel<std::vector<uint64_t>, WHEEL_SIZE>());
	imp_->wheels_.emplace_back(timer_wheel<std::vector<uint64_t>, WHEEL_SIZE>());
	imp_->wheels_.emplace_back(timer_wheel<std::vector<uint64_t>, WHEEL_SIZE>());
}

timer::~timer()
{
	if (nullptr != imp_)
		delete imp_;
}

uint64_t  moon::timer::expired_once(int64_t duration, const timer_handler& callBack)
{
	if (duration < 0)
	{
		throw std::runtime_error("duration must >=0");
	}
	auto pt = std::make_shared<timer_context>(callBack, duration);
	pt->setID(m_inc++);
	pt->setRepeatTimes(1);
	return add_new_timer(pt);
}

uint64_t  moon::timer::repeat(int64_t duration, int32_t times, const timer_handler& callBack)
{
	if (duration < 0)
	{
		throw std::runtime_error("duration must >=0");
	}
	auto pt = std::make_shared<timer_context>(callBack, duration);
	pt->setID(m_inc++);
	pt->setRepeatTimes(times);
	return add_new_timer(pt);
}

void moon::timer::remove(uint64_t timerid)
{
	auto iter = imp_->timers_.find(timerid);
	if (iter != imp_->timers_.end())
	{
		iter->second->setRemoved(true);
		return;
	}

	for (auto nit = imp_->new_timers_.begin(); nit != imp_->new_timers_.end(); nit++)
	{
		if ((*nit)->getID() == timerid)
		{
			(*nit)->setRemoved(true);
			break;
		}
	}
}

void timer::update()
{
	for (auto& it : imp_->new_timers_)
	{
		if(it->getRemoved())
			continue;
		add_timer(it);
	}
	imp_->new_timers_.clear();

	if (m_prewTick == 0)
	{
		m_prewTick = millseconds();
	}

	auto nowTick = millseconds();
	m_tick += (nowTick - m_prewTick);
	m_prewTick = nowTick;

	while(m_tick >= PRECISION)
	{
		m_tick -= PRECISION;
		if(m_Stop)
			continue;

		{
			auto& timers = imp_->wheels_[0].front();
			if (timers.size() > 0)
			{
				expired(timers);
				timers.clear();
			}
			imp_->wheels_[0].pop_front();
		}

		int i = 0;
		for (auto wheel = imp_->wheels_.begin(); wheel != imp_->wheels_.end(); wheel++, i++)
		{
			auto next_wheel = wheel;
			if (imp_->wheels_.end() == (++next_wheel))
				break;

			if (wheel->round())
			{
				auto& timers = next_wheel->front();
				if (timers.size() != 0)
				{
					for (auto it = timers.begin(); it != timers.end(); it++)
					{
						auto slot = get_slot(*it, i);
						(*wheel)[slot].push_back(*it);
					}
					timers.clear();
				}
				next_wheel->pop_front();
			}
		}
	}
}

uint64_t moon::timer::add_new_timer(const timer_context_ptr& t)
{
	//timer id  repeated
	if (imp_->timers_.find(t->getID()) != imp_->timers_.end())
	{
		throw std::runtime_error("got a repeated timer id!");
	}
	t->setEndtime(t->getDuration() + millseconds());
	imp_->new_timers_.push_back(t);
	return t->getID();
}

// slots:      8bit(notuse) 8bit(wheel3_slot)  8bit(wheel2_slot)  8bit(wheel1_slot)  
uint64_t moon::timer::make_key(uint64_t id, uint32_t slots)
{
	return ((uint64_t(id) << 24) | slots);
}

void timer::expired(const std::vector<uint64_t>& timers)
{
	for (auto it = timers.begin(); it != timers.end(); it++)
	{
		uint64_t id = ((*it) >> 24);
		auto iter = imp_->timers_.find(id);
		if (iter != imp_->timers_.end())
		{
			auto tcx = iter->second;
			imp_->timers_.erase(id);
			if (!tcx->getRemoved()&& tcx->getRepeatTimes() != 0)
			{
				tcx->expired();
			
				if (tcx->getRepeatTimes() == -1)
				{
					add_new_timer(tcx);
				}
				else if (tcx->getRepeatTimes() > 0)
				{
					tcx->setRepeatTimes(tcx->getRepeatTimes() - 1);
					add_new_timer(tcx);
				}
			}
		}
	}
}

void timer::add_timer(const timer_context_ptr& t)
{
	auto now = millseconds();
	if (t->getEndtime() > now)
	{
		auto diff = t->getEndtime() - now;
		diff = (diff >= 0) ? diff : 0;

		auto offset = diff%PRECISION;
		//Ð£Õý
		if (offset > 0)
		{
			diff += PRECISION;
		}
		auto slot_count = diff / PRECISION;
		uint64_t key = 0;
		do
		{
			int i = 0; 
			uint32_t slots = 0;
			for (auto it = imp_->wheels_.begin(); it != imp_->wheels_.end(); it++,i++)
			{
				auto& wheel = *it;
				slot_count += wheel.next_slot();
				uint8_t slot = (slot_count - 1) % (wheel.size());
				slot_count -= slot;
				slots |= slot << (i * 8);
				key = make_key(t->getID(), slots);
				if (slot_count < wheel.size())
				{
					//printf("timer id %llu add to wheel [%d] slot [%d]\r\n",t->getID(),  i + 1, slot);
					wheel[slot].push_back(key);
					break;
				}
				slot_count /= wheel.size();
			}
			break;
		} while (0);
		imp_->timers_[t->getID()] = t;
	}
	else
	{
		imp_->wheels_[0].front().push_back(make_key(t->getID(),0));
		imp_->timers_[t->getID()] = t;
		//printf("time out.\r\n");
	}
}

uint8_t moon::timer::get_slot(uint64_t  key, int which_queue)
{
	if (which_queue >= 4)
	{
		throw std::runtime_error("which_queue must < 4");
	}
	return (key >> (which_queue * 8)) & 0xFF;
}

void moon::timer::stop_all_timer()
{
	m_Stop = true;
}

void moon::timer::start_all_timer()
{
	m_Stop = false;
}

int64_t moon::timer::millseconds()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

