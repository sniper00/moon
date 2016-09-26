/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <memory>
#include <vector>
#include <functional>

class SingleThreadLock
{
public:
	void lock()
	{

	}

	void unlock()
	{

	}
};

template<class T>
class GuradLock
{
public:
	GuradLock(T& t)
		:_t(t)
	{
		_t.lock();
	}

	~GuradLock()
	{
		_t.unlock();
	}

private:
	T& _t;
};

template<class T,size_t pool_size = 0, typename Lock = SingleThreadLock>
class ObjectPool
{
public:
	ObjectPool() 
	{

	};

	~ObjectPool()
	{
		GuradLock<Lock> lock(m_Objectlock);
		for (auto& it : m_objects)
		{
			delete it;
		}
	};

	ObjectPool(const ObjectPool& t) = delete;
	ObjectPool& operator=(const ObjectPool& t) = delete;

	template<class... Args>
	std::shared_ptr<T> create(Args... args)
	{
		std::shared_ptr<T> ret = Find();
		if (nullptr == ret)
		{
			ret = std::shared_ptr<T>(new T(args...), std::bind(&ObjectPool::Release, this, std::placeholders::_1));
		}

		if (nullptr != ret)
		{
			ret->init(args...);
		}
		return ret;
	}

	size_t size()
	{
		return m_objects.size();
	}
private:
	std::shared_ptr<T> Find()
	{
		T* t = nullptr;
		{
			GuradLock<Lock> lock(m_Objectlock);
			if (!m_objects.empty())
			{
				t = m_objects.back();
				m_objects.pop_back();
			}
		}

		if (t != nullptr)
		{
			return  std::shared_ptr<T>(t, std::bind(&ObjectPool::Release, this, std::placeholders::_1));
		}
		return nullptr;
	}

	void Release(T* t)
	{
		if (m_objects.size() >= pool_size)
		{
			delete t;
			return;
		}
		GuradLock<Lock> lock(m_Objectlock);
		m_objects.push_back(t);
	}

private:
	Lock						m_Objectlock;
	std::vector<T*>	m_objects;
};