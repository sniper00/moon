/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <mutex>
#include <condition_variable>
#include <deque>
#include <cassert>
#include <atomic>

namespace moon
{
	template<class T,size_t max_size = 50,bool CheckEmpty = false>
	class SyncQueue
	{
	public:
		SyncQueue()
			:m_exit(false)
		{
		}

		SyncQueue(const SyncQueue& t) = delete;
		SyncQueue& operator=(const SyncQueue& t) = delete;

		void PushBack(const T& x)
		{
			std::unique_lock<std::mutex> lck(m_mutex);
			m_notFull.wait(lck, [this] {return m_exit || (m_queue.size() < max_size); });
			m_queue.push_back(x);
			m_notEmpty.notify_one();
		}

		template<typename _Tdata>
		void EmplaceBack(_Tdata&& v)
		{
			std::unique_lock<std::mutex> lck(m_mutex);
			m_queue.emplace_back(std::forward<_Tdata>(v));
		}

		T PopFront()
		{
			std::unique_lock<std::mutex> lck(m_mutex);

			if (CheckEmpty)
			{
				m_notEmpty.wait(lck, [this] {return m_exit || (m_queue.size()>0); });
			}
			else
			{
				if (m_queue.empty())
				{
					return T();
				}
			}

			assert(!m_queue.empty());
			T t(m_queue.front());
			m_queue.pop_front();
			return t;
		}

		size_t Size()
		{
			std::unique_lock<std::mutex> lck(m_mutex);
			return m_queue.size();
		}

		std::deque<T> Move()
		{
			std::unique_lock<std::mutex> lck(m_mutex);
			if (CheckEmpty)
			{
				m_notEmpty.wait(lck, [this] {return m_exit || (m_queue.size() > 0); });
			}
			auto tmp = std::move(m_queue);
			m_notFull.notify_one();
			return std::move(tmp);
		}

		void Exit()
		{
			m_exit = true;
			m_notFull.notify_all();
			m_notEmpty.notify_all();
		}

	private:
		std::mutex											m_mutex;
		std::condition_variable						m_notFull;
		std::condition_variable						m_notEmpty;
		std::deque<T>										m_queue;
		std::atomic_bool									m_exit;
	};

}
