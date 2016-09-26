/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <unordered_map>
#include <memory>
#include <vector>

#include "TimerWheel.hpp"
#include "TimerContext.hpp"

namespace moon
{
	class TimerPool
	{	
	public:
		//每个时间轮子大小，最大255
		static const  uint8_t  wheel_size = 255;
		//精度 ms
		static const int32_t PRECISION = 15;

		TimerPool();

		~TimerPool();
		/**
		* 添加一个只执行一次的计时器
		*
		* @duration 计时器间隔 ms
		* @callBack  回掉函数
		*  typedef   std::function<void()> timer_handler;
		* 返回计时器ID
		*/
		uint32_t  expiredOnce(int64_t duration,const timer_handler& callBack);

		uint32_t  expiredOnce(int64_t duration,timer_handler&& callBack);

		/**
		* 添加重复执行的计时器
		*
		* @duration 计时器间隔 ms
		* @times 重复次数，(-1 无限循环)
		* @callBack 回掉函数
		*  typedef   std::function<void()> timer_handler;
		* 返回计时器ID
		*/
		uint32_t  repeat(int64_t duration, int32_t times, timer_handler&& callBack);

		uint32_t  repeat(int64_t duration, int32_t times, const timer_handler& callBack);

		/**
		* 移除一个计时器
		*
		* @timerid 计时器 ID
		*/
		void remove(uint32_t timerid);
		
		//逻辑线程需要调用这个函数，驱动计时器
		void update();

		void stopAllTimer();

		void startAllTimer();

		static int64_t millseconds();
	private:
		uint32_t 	addNewTimer(const timer_context_ptr& t);
		void		addTimer(const timer_context_ptr& t);
		void		expired(const std::vector<uint64_t>& timers);
		uint64_t	makeKey(uint32_t id, uint32_t  slots);
		uint8_t		getSlot(uint64_t  key, int which_queue);	
	private:
		std::vector < TimerWheel<std::vector<uint64_t>, wheel_size>> m_wheels;
		std::unordered_map<uint32_t, timer_context_ptr>		m_timers;
		std::vector<timer_context_ptr>										m_new;
		int64_t																				m_tick;
		uint32_t																			m_inc;
		bool																					m_Stop;
	};
}

