/****************************************************************************

Git <https://github.com/sniper00/moon_net>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <cassert>

namespace moon
{
	template<typename T>
	class Singleton
	{
	public:
		Singleton(void)
		{
			assert(!m_Singleton &&"Singleton already exist!!!");
			m_Singleton = static_cast<T*>(this);
		}

		~Singleton()
		{
			assert(m_Singleton);
			m_Singleton = nullptr;
		}

		static T& Instance()
		{
			assert(m_Singleton);
			return (*m_Singleton);
		}

		static T* InstancePtr()
		{
			return m_Singleton;
		}

	private:
		static T* m_Singleton;
	};

	template <typename T>
	T* Singleton<T>::m_Singleton = nullptr;
}

