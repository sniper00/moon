/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <cassert>

namespace moon
{
	template<typename T>
	class singleton
	{
	public:
		singleton(void)
		{
			assert(!singleton_ &&"Singleton already exist!!!");
			singleton_ = static_cast<T*>(this);
		}

		~singleton()
		{
			assert(singleton_);
			singleton_ = nullptr;
		}

		static T& instance()
		{
			assert(singleton_);
			return (*singleton_);
		}

		static T* instance_ptr()
		{
			return singleton_;
		}

	private:
		static T* singleton_;
	};

	template <typename T>
	T* singleton<T>::singleton_ = nullptr;
}

