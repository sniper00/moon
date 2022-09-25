#pragma once
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cassert>
#include <type_traits>

namespace moon
{
    //Unbounded queue
    template<class T, typename Lock = std::mutex, template <typename Elem,typename = std::allocator<Elem>> class Container = std::vector>
    class concurrent_queue
    {
    public:
        static_assert(std::is_nothrow_move_constructible_v<T>);
        static_assert(std::is_nothrow_move_assignable_v<T>);

        using container_type = Container<T>;
        using lock_type = Lock;

        concurrent_queue() = default;
        concurrent_queue(const concurrent_queue& t) = delete;
        concurrent_queue& operator=(const concurrent_queue& t) = delete;

        template<typename TData>
        size_t push_back(TData&& x)
        {
            std::lock_guard<lock_type> lk(mutex_);
            container_.push_back(std::forward<TData>(x));
            return container_.size();
        }

        bool try_pop(T& t)
        {
            std::lock_guard<lock_type> lk(mutex_);
            if (container_.empty())
                return false;
            t = std::move(container_.front());
            container_.pop_front();
            return true;
        }

        size_t size() const
        {
            std::lock_guard<lock_type> lk(mutex_);
            return container_.size();
        }

        size_t capacity() const
        {
            std::lock_guard<lock_type> lk(mutex_);
            return container_.capacity();
        }

        bool try_swap(container_type& other)
        {
            std::lock_guard<lock_type> lk(mutex_);
            if (container_.empty())
                return false;
            container_.swap(other);
            return true;
        }
    private:
        mutable lock_type mutex_;
        container_type container_;
    };
}
