/****************************************************************************

Git <https://github.com/sniper00/moon_net>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <cstdint>
#include <cassert>

namespace moon
{
	template<typename _Type, uint8_t _Size>
	class TimerWheel
	{
	public:
		TimerWheel()
			:m_head(1)
		{
		}

		_Type& operator[](uint8_t pos)
		{
			assert(pos < _Size);
			return m_array[pos];
		}

		_Type& front()
		{
			assert(m_head < _Size);
			return m_array[m_head];
		}

		void pop_front()
		{
			auto tmp = ++m_head;
			m_head = tmp % _Size;
		}

		bool round()
		{
			return m_head == 0;
		}

		uint8_t size()
		{
			return _Size;
		}

		size_t next_slot() { return m_head; }
	private:
		_Type		m_array[_Size];
		uint32_t	m_head;
	};
};

