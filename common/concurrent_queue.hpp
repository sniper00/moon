#pragma once
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cassert>
#include <atomic>
#include <type_traits>

namespace moon
{
    template<bool enable>
    struct queue_condition_variable
    {
        std::condition_variable cv_;
        template<typename TLock>
        void wait(TLock& lock)
        {
            if constexpr(enable)
                cv_.wait(lock);
        }

        void notify_one()
        {
            if constexpr(enable)
                cv_.notify_one();
        }

        void notify_all()
        {
            if constexpr(enable)
                cv_.notify_all();
        }
    };

    template<class T, typename Lock = std::mutex, template <typename Elem,typename = std::allocator<Elem>> class Container = std::vector, bool BlockEmpty = false, bool BlockFull = false>
    class concurrent_queue
    {
    public:
        using container_type = Container<T>;
        using lock_type = Lock;
        using empty_cv_type = queue_condition_variable<BlockEmpty>;
        using full_cv_type = queue_condition_variable<BlockFull>;
        using lock_guard_type =  std::conditional_t<BlockEmpty || BlockFull,std::unique_lock<lock_type>,std::lock_guard<lock_type>>;

        concurrent_queue()
            :max_size_(std::numeric_limits<size_t>::max())
        {
        }

        concurrent_queue(const concurrent_queue& t) = delete;
        concurrent_queue& operator=(const concurrent_queue& t) = delete;

        void set_max_size(size_t max_size)
        {
            max_size_ = max_size;
        }

        template<typename TData>
        size_t push_back(TData&& x)
        {
            lock_guard_type lck(mutex_);
            if(container_.size()>=max_size_)
                full_cv_.wait(lck);
            container_.push_back(std::forward<TData>(x));
            empty_cv_.notify_one();
            return container_.size();
        }

        bool try_pop(T& t)
        {
            lock_guard_type lck(mutex_);
            if (container_.empty())
                return false;

            t = std::move(container_.front());
            container_.pop_front();
            full_cv_.notify_one();
            return true;
        }

        size_t size() const
        {
            lock_guard_type lck(mutex_);
            return container_.size();
        }

        size_t capacity() const
        {
            lock_guard_type lck(mutex_);
            return container_.capacity();
        }

        void swap(container_type& other)
        {
            lock_guard_type lck(mutex_);
            if(container_.empty())
                empty_cv_.wait(lck);
            container_.swap(other);
            full_cv_.notify_one();
        }

        void exit()
        {
            lock_guard_type lck(mutex_);
            full_cv_.notify_all();
            empty_cv_.notify_all();
        }
    private:
        mutable lock_type mutex_;
        empty_cv_type empty_cv_;
        full_cv_type full_cv_;
        container_type container_;
        size_t max_size_;
    };

}
