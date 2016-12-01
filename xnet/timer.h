/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include "macro_define.h"

namespace moon
{
	typedef   std::function<void()> timer_handler;

	class timer_context;
	typedef std::shared_ptr<timer_context> timer_context_ptr;

	class XNET_DLL timer
	{
	public:
		//每个时间轮子大小，最大255
		static const  uint8_t  WHEEL_SIZE = 255;
		//精度 ms
		static const int32_t PRECISION = 10;

		timer();

		~timer();
		/**
		* 添加一个只执行一次的计时器
		*
		* @duration 计时器间隔 ms
		* @callBack  回掉函数
		*  typedef   std::function<void()> timer_handler;
		* 返回计时器ID
		*/
		uint64_t  expired_once(int64_t duration, const timer_handler& callBack);

		/**
		* 添加重复执行的计时器
		*
		* @duration 计时器间隔 ms
		* @times 重复次数，(-1 无限循环)
		* @callBack 回掉函数
		*  typedef   std::function<void()> timer_handler;
		* 返回计时器ID
		*/
		uint64_t  repeat(int64_t duration, int32_t times, const timer_handler& callBack);

		/**
		* 移除一个计时器
		*
		* @timerid 计时器 ID
		*/
		void			remove(uint64_t timerid);

		//逻辑线程需要调用这个函数，驱动计时器
		void			update();

		void			stop_all_timer();

		void			start_all_timer();

		static int64_t millseconds();
	private:
		uint64_t 	add_new_timer(const timer_context_ptr& t);
		void			add_timer(const timer_context_ptr& t);
		void			expired(const std::vector<uint64_t>& timers);
		uint64_t	make_key(uint64_t id, uint32_t  slots);
		uint8_t		get_slot(uint64_t  key, int which_queue);
	private:
		struct timer_pool_imp;
		timer_pool_imp* imp_;
	};
}

