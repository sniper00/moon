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
	template<class T,bool CheckEmpty = false>
	class sync_queue
	{
	public:
		sync_queue()
			:exit_(false)
			,max_size_(std::numeric_limits<size_t>::max())
		{
		}

		sync_queue(const sync_queue& t) = delete;
		sync_queue& operator=(const sync_queue& t) = delete;

		void set_max_size(size_t max_size)
		{
			max_size_ = max_size;
		}

		template<typename TData>
		void push_back(TData&& x)
		{		
			std::unique_lock<std::mutex> lck(mutex_);
			notfull_.wait(lck, [this] {		
				return exit_ || (queue_.size() < max_size_);
			});

			queue_.push_back(std::forward<TData>(x));	
			notempty_.notify_one();
		}

		T pop_front()
		{
			std::unique_lock<std::mutex> lck(mutex_);

			if (CheckEmpty)
			{
				notempty_.wait(lck, [this] {
					return exit_ || (queue_.size()>0); 
				});
			}
			else
			{
				if (queue_.empty())
				{
					return T();
				}
			}

			assert(!queue_.empty());
			T t(queue_.front());
			queue_.pop_front();
			return t;
		}

		size_t size()
		{
			std::unique_lock<std::mutex> lck(mutex_);
			return queue_.size();
		}

		std::deque<T> move()
		{
			std::unique_lock<std::mutex> lck(mutex_);
			if (CheckEmpty)
			{
				notempty_.wait(lck, [this] {return exit_ || (queue_.size() > 0); });
			}

			auto tmp = std::move(queue_);
			notfull_.notify_one();
			return std::move(tmp);
		}

		void exit()
		{
			exit_ = true;
			notfull_.notify_all();
			notempty_.notify_all();
		}

	private:
		std::mutex mutex_;
		std::condition_variable notfull_;
		std::condition_variable notempty_;
		std::deque<T> queue_;
		std::atomic_bool exit_;
		size_t max_size_;
	};

}
