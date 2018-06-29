/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cassert>
#include <atomic>

namespace moon
{
	template<bool v>
	struct queue_empty_check{};

	template<>
	struct queue_empty_check<true>
	{
		std::condition_variable notempty_;
		template<typename TLock,typename TCond>
		void check(TLock& lock, TCond&& cond)
		{
			notempty_.wait(lock, std::forward<TCond>(cond));
		}

		void notify_one()
		{
			notempty_.notify_one();
		}

		void notify_all()
		{
			notempty_.notify_all();
		}
	};

	template<>
	struct queue_empty_check<false>
	{
		template<typename TLock, typename TCond>
		void check(TLock&, TCond&&)
		{
		}

		void notify_one()
		{
		}

		void notify_all()
		{
		}
	};

	template<bool v>
	struct queue_full_check{};

	template<>
	struct queue_full_check<true>
	{
		std::condition_variable notfull_;

		template<typename TLock,typename TCond>
		void check(TLock& lock, TCond&& cond)
		{
			notfull_.wait(lock, std::forward<TCond>(cond));
		}

		void notify_one()
		{
			notfull_.notify_one();
		}

		void notify_all()
		{
			notfull_.notify_all();
		}
	};

	template<>
	struct queue_full_check<false>
	{
		template<typename TLock, typename TCond>
		void check(TLock&, TCond&&)
		{
		}

		void notify_one()
		{
		}

		void notify_all()
		{
		}
	};

	template<class T,typename LockType = std::mutex, bool CheckEmpty = false,bool CheckFull = false>
	class concurrent_queue :public queue_empty_check<CheckEmpty>,public queue_full_check<CheckFull>
	{
	public:
		using queue_type = std::vector<T>;
		using lock_t = LockType;
		using empty_check_t = queue_empty_check<CheckEmpty>;
		using full_check_t = queue_full_check<CheckFull>;

		concurrent_queue()
			:exit_(false)
			,max_size_(std::numeric_limits<size_t>::max())
		{
		}

		concurrent_queue(const concurrent_queue& t) = delete;
		concurrent_queue& operator=(const concurrent_queue& t) = delete;

		void set_max_size(size_t max_size) const
		{
			max_size_ = max_size;
		}

		template<typename TData>
		size_t push_back(TData&& x)
		{		
			std::unique_lock<lock_t> lck(mutex_);

			full_check_t::check(lck, [this] {
				return exit_ || (queue_.size() < max_size_);
			});

			queue_.push_back(std::forward<TData>(x));

			empty_check_t::notify_one();

            return queue_.size();
		}

		bool try_pop(T& t)
		{
			std::unique_lock<lock_t> lck(mutex_);
			empty_check_t::check(lck, [this] {
				return exit_ || (queue_.size()>0);
			});
			if (queue_.empty())
			{
				return false;
			}		
            t = queue_.front();
			queue_.pop_front();
			full_check_t::notify_one();
			return true;
		}

		size_t size() const
		{
			std::unique_lock<lock_t> lck(mutex_);
			return queue_.size();
		}

		void  swap(queue_type& other)
		{
			std::unique_lock<lock_t> lck(mutex_);
			empty_check_t::check(lck, [this] {
				return exit_ || (queue_.size()>0);
			});
			queue_.swap(other);
			full_check_t::notify_one();
		}

		void exit()
		{
            std::unique_lock<lock_t> lck(mutex_);
            exit_ = true;
            full_check_t::notify_all();
            empty_check_t::notify_all();
		}

	private:
		mutable lock_t mutex_;
		queue_type queue_;
		std::atomic_bool exit_;
		size_t max_size_;
	};

}
