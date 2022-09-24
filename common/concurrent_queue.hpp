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
        template<typename Lock, typename Predicate>
        void wait(Lock& lock, Predicate pred)
        {
            if constexpr(enable)
                cv_.wait(lock, pred);
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
        using consumer_cv_type = queue_condition_variable<BlockEmpty>;
        using producer_cv_type = queue_condition_variable<BlockFull>;
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
            size_t n = 0;
            {
                lock_guard_type lock(mutex_);
                if (container_.size() >= max_size_)
                    producer_cv_.wait(lock, [this] {
                            return container_.size() < max_size_;
                        });
                container_.push_back(std::forward<TData>(x));
                n = container_.size();
            }
            consumer_cv_.notify_one();
            return n;
        }

        bool try_pop(T& t)
        {
            {
                lock_guard_type lck(mutex_);
                if (container_.empty())
                    return false;
                t = std::move(container_.front());
                container_.pop_front();
            }
            producer_cv_.notify_one();
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
            {
                lock_guard_type lck(mutex_);
                if (container_.empty())
                    consumer_cv_.wait(lck, [this] {
                            return !container_.empty();
                        });
                container_.swap(other);
            }
            producer_cv_.notify_one();
        }

        bool try_swap(container_type& other)
        {
            {
                lock_guard_type lck(mutex_);
                if (container_.empty())
                    return false;
                container_.swap(other);
            }
            producer_cv_.notify_one();
            return true;
        }

        void exit()
        {
            producer_cv_.notify_all();
            consumer_cv_.notify_all();
        }
    private:
        mutable lock_type mutex_;
        producer_cv_type producer_cv_;
        consumer_cv_type consumer_cv_;
        container_type container_;
        size_t max_size_;
    };

}
