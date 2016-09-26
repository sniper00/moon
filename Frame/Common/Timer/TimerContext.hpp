/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <functional>
#include <cstdint>
#include <cassert>
#include <memory>
namespace moon
{
	typedef   std::function<void()> timer_handler;

	class TimerContext;
	typedef std::shared_ptr<TimerContext> timer_context_ptr;

	class TimerContext
	{
	public:
		TimerContext(const timer_handler& callBack, int64_t duration)
			:m_handler(callBack)
			, m_id(0)
			, m_endtime(0)
			, m_duration(duration)
			, m_repeatTimes(0)
			, m_removed(false)
		{

		}

		TimerContext(timer_handler&& callBack, int64_t duration)
			:m_handler(callBack)
			, m_id(0)
			, m_endtime(0)
			, m_duration(duration)
			, m_repeatTimes(0)
			, m_removed(false)
		{
		}

		void init(timer_handler&& callBack, int64_t duration)
		{
			m_handler = std::move(callBack);
			m_endtime = 0;
			m_repeatTimes = 0;
			m_removed = false;
			m_duration = duration;
		}

		void init(const timer_handler& callBack, int64_t duration)
		{
			m_handler = callBack;
			m_endtime = 0;
			m_repeatTimes = 0;
			m_removed = false;
			m_duration = duration;
		}

		void setID(uint32_t value)
		{
			m_id = value;
		}

		uint32_t getID()
		{
			return m_id;
		}

		void setEndtime(int64_t value)
		{
			m_endtime = value;
		}

		int64_t getEndtime()
		{
			return m_endtime;
		}

		int64_t getDuration()
		{
			return m_duration;
		}

		void setRepeatTimes(int32_t value)
		{
			m_repeatTimes = value;
		}

		int32_t getRepeatTimes()
		{
			return m_repeatTimes;
		}

		void  expired()
		{
			assert(nullptr != m_handler);
			m_handler();
		}

		void setRemoved(bool value)
		{
			m_removed = value;
		}

		bool getRemoved()
		{
			return m_removed;
		}
	private:
		timer_handler				m_handler;//调度器回掉
		uint32_t						m_id;
		int64_t							m_endtime;
		int64_t							m_duration;
		int32_t							m_repeatTimes;//重复次数，-1循环
		bool								m_removed;
	};
}

