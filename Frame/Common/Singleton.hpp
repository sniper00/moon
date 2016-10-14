/****************************************************************************
Copyright (c) 2016 moon

http://blog.csdn.net/greatchina01

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
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

