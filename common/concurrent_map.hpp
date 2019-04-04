#pragma once
#include<unordered_map>
#include<string>
#include<memory>
#include "noncopyable.hpp"
#include "macro_define.hpp"
#include "rwlock.hpp"

namespace moon
{
    namespace detail
    {
        template<class TKey, class TValue>
        struct try_get_value
        {
            template<class TMap>
            static bool get(const TMap& map, const TKey& key, TValue& value)
            {
                auto iter = map.find(key);
                if (iter != map.end())
                {
                    value = iter->second;
                    return true;
                }
                return false;
            }
        };
    }

    template<typename Key, typename Value, typename Lock = std::shared_timed_mutex >
    class concurrent_map : noncopyable
    {
    public:
        concurrent_map() = default;

        template<class TKey, class TValue>
        void set(TKey&& key, TValue&& value)
        {
            std::unique_lock lck(lock_);
            auto res = data_.try_emplace(std::forward<TKey>(key), value);
            auto iter = data_.find(key);
            if (!res.second)
            {
                res.first->second = std::forward<TValue>(value);
            }
        }

        template<class TKey, class TValue>
        bool try_set(TKey&& key, TValue&& value)
        {
            std::unique_lock lck(lock_);
            auto res = data_.try_emplace(std::forward<TKey>(key), std::forward<TValue>(value));
            return res.second;
        }

        bool try_get_value(const Key& key, Value& value) const
        {
            std::shared_lock lck(lock_);
            return detail::try_get_value<Key, Value>::get(data_, key, value);
        }

        bool erase(const Key& key)
        {
            std::unique_lock lck(lock_);
            auto iter = data_.find(key);
            if (iter != data_.end())
            {
                data_.erase(iter);
                return true;
            }
            return false;
        }

        void clear()
        {
            std::unique_lock lck(lock_);
            data_.clear();
        }

        size_t size() const
        {
            std::shared_lock lck(lock_);
            return data_.size();
        }

        bool has(const Key& key) const
        {
            std::shared_lock lck(lock_);
            auto iter = data_.find(key);
            if (iter != data_.end())
            {
                return true;
            }
            return false;
        }
    private:
        mutable Lock lock_;
        std::unordered_map<Key, Value> data_;
    };
}

