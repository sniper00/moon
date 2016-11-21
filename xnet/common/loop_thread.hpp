/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <cstdint>
#include <chrono>
#include <thread>
#include <atomic>

namespace moon
{
	class loop_thread
	{
	public:
		loop_thread()
			:interval_(1000), stop_(false)
		{

		}

		~loop_thread()
		{

		}

		loop_thread(const loop_thread& t) = delete;
		loop_thread& operator=(const loop_thread& t) = delete;

		//…Ë÷√—≠ª∑º‰∏Ù ∫¡√Î
		void set_interval(uint32_t interval)
		{
			interval_ = interval;
		}

		void run()
		{
			thread_ = std::thread(std::bind(&loop_thread::loop, this));
		}

		void stop()
		{
			stop_ = true;
			thread_.join();
		}

		std::function<void(uint32_t)> on_update;
	private:
		void loop()
		{
			uint64_t prew_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			uint32_t prev_sleep_time = 0;
			while (!stop_)
			{
				uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				uint32_t diff = (now - prew_time) & 0xFFFFFFFF;
				prew_time = now;

				if (on_update != nullptr)
				{
					on_update(diff);
				}

				if (diff <= interval_ + prev_sleep_time)
				{
					prev_sleep_time = interval_ + prev_sleep_time - diff;
					std::this_thread::sleep_for(std::chrono::milliseconds(prev_sleep_time));
				}
				else
					prev_sleep_time = 0;
			}
		}

	private:
		uint32_t						interval_;
		std::thread					thread_;
		std::atomic_bool 			stop_;
	};
}
