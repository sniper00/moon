/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <cstdint>
#include <cassert>
#include <vector>
#include <mutex>
#include <functional>

namespace moon
{
	class nonlock
	{
	public:
		void lock()
		{
		}
		void unlock()
		{
		}
	};

    template<class T, size_t PoolSize = 0, typename TLock = nonlock>
    class pointer_pool
    {
    public:
        using lock_t = TLock;
        using object_t = T;
        using object_pointer_t = T*;

        pointer_pool()
        {
        }

        pointer_pool(const pointer_pool&) = delete;
        pointer_pool& operator=(const pointer_pool&) = delete;

        ~pointer_pool()
        {
            free_all();
        };

        void release(object_pointer_t t)
        {
            if ((PoolSize!=0) && objects_.size() >= PoolSize)
            {
                delete t;
                return;
            }
            std::unique_lock<lock_t> lk(lock_);
            objects_.push_back(t);
        }

        size_t size() const noexcept
        {
            return objects_.size();
        }

        template<class... Args>
        object_pointer_t create(Args&&... args)
        {
            object_pointer_t t = nullptr;
            {
                std::unique_lock<lock_t> lk(lock_);
                if (!objects_.empty())
                {
                    t = objects_.back();
                    objects_.pop_back();
                }
            }

            if (nullptr == t)
            {
                t = new T(std::forward<Args>(args)...);
            }
            else
            {
                t->init(std::forward<Args>(args)...);
            }
            return t;
        }

        void free_all()
        {
            std::unique_lock<lock_t> lk(lock_);
            for (auto& obj : objects_)
            {
                delete obj;
            }
            objects_.clear();
        }
 
    private:
        std::vector<object_pointer_t> objects_;
        lock_t	 lock_;
    };

    template<class T, size_t PoolSize = 0, typename TLock = nonlock>
    class shared_pointer_pool
    {
    public:
        using pointer_pool_t = pointer_pool<T, PoolSize, TLock>;

        shared_pointer_pool()
            :pool_()
        {
            release_ = std::bind(&pointer_pool_t::release,&pool_,std::placeholders::_1);
        }

        shared_pointer_pool(const shared_pointer_pool&) = delete;
        shared_pointer_pool& operator=(const shared_pointer_pool&) = delete;

        template<class... Args>
        std::shared_ptr<T> create(Args&&... args)
        {
            auto p = pool_.create(std::forward<Args>(args)...);
            if (nullptr != p)
            {
                return  std::shared_ptr<T>(p, release_);
            }
            return nullptr;
        }

        size_t size() const noexcept
        {
            return pool_.size();
        }
    private:
        std::function<void(typename pointer_pool_t::object_pointer_t)> release_;
        pointer_pool_t pool_;
    };
};

