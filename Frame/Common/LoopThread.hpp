/****************************************************************************

Git <https://github.com/sniper00/moon_net>
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
	class LoopThread
	{
	public:
		LoopThread()
			:_Interval(1000), _bStop(false)
		{

		}

		~LoopThread()
		{

		}

		LoopThread(const LoopThread& t) = delete;
		LoopThread& operator=(const LoopThread& t) = delete;

		//…Ë÷√—≠ª∑º‰∏Ù ∫¡√Î
		void Interval(uint32_t interval)
		{
			_Interval = interval;
		}

		void Run()
		{
			_Thread = std::thread(std::bind(&LoopThread::loop, this));
		}

		void Stop()
		{
			_bStop = true;
			_Thread.join();
		}

		std::function<void(uint32_t)> onUpdate;
	private:
		void loop()
		{
			uint64_t prew = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			uint32_t prevSleepTime = 0;
			while (!_bStop)
			{
				uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				uint32_t diff = (now - prew) & 0xFFFFFFFF;
				prew = now;

				if (onUpdate != nullptr)
				{
					onUpdate(diff);
				}

				if (diff <= _Interval + prevSleepTime)
				{
					prevSleepTime = _Interval + prevSleepTime - diff;
					std::this_thread::sleep_for(std::chrono::milliseconds(prevSleepTime));
				}
				else
					prevSleepTime = 0;
			}
		}

	private:
		uint32_t						_Interval;
		std::thread					_Thread;
		std::atomic_bool 			_bStop;
	};
}
