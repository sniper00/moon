/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <cstdint>
#include <chrono>
#include <ctime>
#include <cassert>

namespace moon
{
	class time
	{
	public:
		static int64_t second()
		{
			return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		}

		static int64_t millsecond()
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		}

		static time_t make_time(int year, int month, int day, int hour, int min, int sec)
		{
			//assert(year >= 1990);
			//assert(month>0 && month<=12);
			//assert(day >0 && day <=31);
			//assert(hour >=0 && hour <24);
			//assert(min >=0 && min <60);
			//assert(sec >= 0 && sec <60);

			tm _tm;
			_tm.tm_year = (year - 1900);
			_tm.tm_mon = (month - 1);
			_tm.tm_mday = day;
			_tm.tm_hour = hour;
			_tm.tm_min = min;
			_tm.tm_sec = sec;
			_tm.tm_isdst = 0;

			return mktime(&_tm);
		}
	};
};

