/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#include "TimerPool.h"
#include <cassert>
#include <chrono>

using namespace moon;

TimerPool::TimerPool()
:m_tick(0),m_inc(1), m_Stop(false)
{
	m_wheels.emplace_back(TimerWheel<std::vector<uint64_t>, wheel_size>());
	m_wheels.emplace_back(TimerWheel<std::vector<uint64_t>, wheel_size>());
	m_wheels.emplace_back(TimerWheel<std::vector<uint64_t>, wheel_size>());
	m_wheels.emplace_back(TimerWheel<std::vector<uint64_t>, wheel_size>());
}

TimerPool::~TimerPool()
{
}

uint64_t  moon::TimerPool::ExpiredOnce(int64_t duration, const timer_handler& callBack)
{
	if (duration < 0)
	{
		throw std::runtime_error("duration must >=0");
	}
	auto pt = std::make_shared<TimerContext>(callBack, duration);
	pt->setID(m_inc++);
	pt->setRepeatTimes(1);
	return addNewTimer(pt);
}

uint64_t  moon::TimerPool::Repeat(int64_t duration, int32_t times, const timer_handler& callBack)
{
	if (duration < 0)
	{
		throw std::runtime_error("duration must >=0");
	}
	auto pt = std::make_shared<TimerContext>(callBack, duration);
	pt->setID(m_inc++);
	pt->setRepeatTimes(times);
	return addNewTimer(pt);
}

void moon::TimerPool::Remove(uint64_t timerid)
{
	auto iter = m_timers.find(timerid);
	if (iter != m_timers.end())
	{
		iter->second->setRemoved(true);
		return;
	}

	for (auto nit = m_new.begin(); nit != m_new.end(); nit++)
	{
		if ((*nit)->getID() == timerid)
		{
			(*nit)->setRemoved(true);
			break;
		}
	}
}

void TimerPool::Update()
{
	for (auto& it : m_new)
	{
		if(it->getRemoved())
			continue;
		addTimer(it);
	}
	m_new.clear();

	static auto prewTick = millseconds();
	auto nowTick = millseconds();
	m_tick += (nowTick - prewTick);
	prewTick = nowTick;

	while(m_tick >= PRECISION)
	{
		m_tick -= PRECISION;
		if(m_Stop)
			continue;

		{
			auto& timers = m_wheels[0].front();
			if (timers.size() > 0)
			{
				expired(timers);
				timers.clear();
			}
			m_wheels[0].pop_front();
		}

		int i = 0;
		for (auto wheel = m_wheels.begin(); wheel != m_wheels.end(); wheel++, i++)
		{
			auto next_wheel = wheel;
			if (m_wheels.end() == (++next_wheel))
				break;

			if (wheel->round())
			{
				auto& timers = next_wheel->front();
				if (timers.size() != 0)
				{
					for (auto it = timers.begin(); it != timers.end(); it++)
					{
						auto slot = getSlot(*it, i);
						(*wheel)[slot].push_back(*it);
					}
					timers.clear();
				}
				next_wheel->pop_front();
			}
		}
	}
}

uint64_t moon::TimerPool::addNewTimer(const timer_context_ptr& t)
{
	//timer id  repeated
	if (m_timers.find(t->getID()) != m_timers.end())
	{
		throw std::runtime_error("got a repeated timer id!");
	}
	t->setEndtime(t->getDuration() + millseconds());
	m_new.push_back(t);
	return t->getID();
}

// slots:      8bit(notuse) 8bit(wheel3_slot)  8bit(wheel2_slot)  8bit(wheel1_slot)  
uint64_t moon::TimerPool::makeKey(uint64_t id, uint32_t slots)
{
	return ((uint64_t(id) << 24) | slots);
}

void TimerPool::expired(const std::vector<uint64_t>& timers)
{
	for (auto it = timers.begin(); it != timers.end(); it++)
	{
		uint64_t id = ((*it) >> 24);
		auto iter = m_timers.find(id);
		if (iter != m_timers.end())
		{
			auto tcx = iter->second;
			m_timers.erase(id);
			if (!tcx->getRemoved()&& tcx->getRepeatTimes() != 0)
			{
				tcx->expired();
			
				if (tcx->getRepeatTimes() == -1)
				{
					addNewTimer(tcx);
				}
				else if (tcx->getRepeatTimes() > 0)
				{
					tcx->setRepeatTimes(tcx->getRepeatTimes() - 1);
					addNewTimer(tcx);
				}
			}
		}
	}
}

void TimerPool::addTimer(const timer_context_ptr& t)
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
			for (auto it = m_wheels.begin(); it != m_wheels.end(); it++,i++)
			{
				auto& wheel = *it;
				slot_count += wheel.next_slot();
				uint8_t slot = (slot_count - 1) % (wheel.size());
				slot_count -= slot;
				slots |= slot << (i * 8);
				key = makeKey(t->getID(), slots);
				if (slot_count < wheel.size())
				{
					//printf("TimerPool id %llu add to wheel [%d] slot [%d]\r\n",t->getID(),  i + 1, slot);
					wheel[slot].push_back(key);
					break;
				}
				slot_count /= wheel.size();
			}
			break;
		} while (0);
		m_timers[t->getID()] = t;
	}
	else
	{
		m_wheels[0].front().push_back(makeKey(t->getID(),0));
		m_timers[t->getID()] = t;
		//printf("time out.\r\n");
	}
}

uint8_t moon::TimerPool::getSlot(uint64_t  key, int which_queue)
{
	if (which_queue >= 4)
	{
		throw std::runtime_error("which_queue must < 4");
	}
	return (key >> (which_queue * 8)) & 0xFF;
}

void moon::TimerPool::StopAllTimer()
{
	m_Stop = true;
}

void moon::TimerPool::StartAllTimer()
{
	m_Stop = false;
}

int64_t moon::TimerPool::millseconds()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

