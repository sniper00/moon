#pragma once
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cassert>
#include <atomic>
#include <type_traits>

namespace moon
{
    template<bool v>
    struct queue_block_empty :std::false_type {};

    template<>
    struct queue_block_empty<true> :std::true_type
    {
        std::condition_variable notempty_;
        template<typename TLock, typename TCond>
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

    template<bool v>
    struct queue_block_full :std::false_type {};

    template<>
    struct queue_block_full<true> :std::true_type
    {
        std::condition_variable notfull_;

        template<typename TLock, typename TCond>
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

    template<class T, typename LockType = std::mutex, template <class,class = std::allocator<T>> class Container = std::vector, bool BlockEmpty = false, bool BlockFull = false>
    class concurrent_queue :public queue_block_empty<BlockEmpty>, public queue_block_full<BlockFull>
    {
    public:
        using container_type = Container<T>;
        using lock_t = LockType;
        using block_empty = queue_block_empty<BlockEmpty>;
        using block_full = queue_block_full<BlockFull>;
        static constexpr bool use_cv = std::is_same_v< typename block_empty::type, std::true_type> || std::is_same_v< typename block_full::type, std::true_type>;
        using raii_lock_t =  std::conditional_t<use_cv,std::unique_lock<lock_t>,std::lock_guard<lock_t>>;

        concurrent_queue()
            :exit_(false)
            , max_size_(std::numeric_limits<size_t>::max())
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
            raii_lock_t lck(mutex_);

            if constexpr (std::is_same_v< typename block_full::type, std::true_type>)
            {
                block_full::check(lck, [this] {
                    return (container_.size() < max_size_) || exit_;
                });
            }

            container_.push_back(std::forward<TData>(x));

            if constexpr (std::is_same_v< typename block_empty::type, std::true_type>)
            {
                block_empty::notify_one();
            }
            return container_.size();
        }

        bool try_pop(T& t)
        {
            raii_lock_t lck(mutex_);
            if constexpr (std::is_same_v< typename block_empty::type, std::true_type>)
            {
                block_empty::check(lck, [this] {
                    return (container_.size() > 0) || exit_;
                });
            }
            if (container_.empty())
            {
                return false;
            }
            t = container_.front();
            container_.pop_front();
            if constexpr (std::is_same_v< typename block_full::type, std::true_type>)
            {
                block_full::notify_one();
            }
            return true;
        }

        size_t size() const
        {
            raii_lock_t lck(mutex_);
            return container_.size();
        }

        size_t capacity() const
        {
            raii_lock_t lck(mutex_);
            return container_.capacity();
        }

        void  swap(container_type& other)
        {
            raii_lock_t lck(mutex_);
            if constexpr (std::is_same_v< typename block_empty::type, std::true_type>)
            {
                block_empty::check(lck, [this] {
                    return (container_.size() > 0) || exit_;
                });
            }
            container_.swap(other);
            if constexpr (std::is_same_v< typename block_full::type, std::true_type>)
            {
                block_full::notify_one();
            }
        }

        void exit()
        {
            raii_lock_t lck(mutex_);
            exit_ = true;
            if constexpr (std::is_same_v< typename block_full::type, std::true_type>)
            {
                block_full::notify_all();
            }
            if constexpr (std::is_same_v< typename block_empty::type, std::true_type>)
            {
                block_empty::notify_all();
            }
        }

    private:
        mutable lock_t mutex_;
        container_type container_;
        std::atomic_bool exit_;
        size_t max_size_;
    };

}
