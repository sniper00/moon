#pragma once
#include "common.hpp"
#include "noncopyable.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace moon {
namespace detail {
    template<class TKey, class TValue>
    struct try_get_value {
        template<class TMap>
        static bool get(const TMap& map, const TKey& key, TValue& value) {
            if (auto iter = map.find(key); iter != map.end()) {
                value = iter->second;
                return true;
            }
            return false;
        }
    };
} // namespace detail

template<typename Key, typename Value, typename Lock = std::shared_timed_mutex>
class concurrent_map: private noncopyable {
public:
    concurrent_map() = default;

    template<class TKey, class TValue>
    void set(TKey&& key, TValue&& value) {
        std::unique_lock lck(lock_);
        auto res = data_.try_emplace(std::forward<TKey>(key), value);
        if (!res.second) {
            res.first->second = std::forward<TValue>(value);
        }
    }

    template<class TKey, class TValue>
    bool try_set(TKey&& key, TValue&& value) {
        std::unique_lock lck(lock_);
        auto res = data_.try_emplace(std::forward<TKey>(key), std::forward<TValue>(value));
        return res.second;
    }

    bool try_get_value(const Key& key, Value& value) const {
        std::shared_lock lck(lock_);
        return detail::try_get_value<Key, Value>::get(data_, key, value);
    }

    bool erase(const Key& key) {
        std::unique_lock lck(lock_);
        if (auto iter = data_.find(key); iter != data_.end()) {
            data_.erase(iter);
            return true;
        }
        return false;
    }

    void clear() {
        std::unique_lock lck(lock_);
        data_.clear();
    }

    size_t size() const {
        std::shared_lock lck(lock_);
        return data_.size();
    }

    bool has(const Key& key) const {
        std::shared_lock lck(lock_);
        if (auto iter = data_.find(key); iter != data_.end()) {
            return true;
        }
        return false;
    }

private:
    mutable Lock lock_;
    std::unordered_map<Key, Value> data_;
};
} // namespace moon
