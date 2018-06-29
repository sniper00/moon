/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

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
    
    template<typename Key,typename Value,typename Lock = std::shared_timed_mutex >
    class concurrent_map : noncopyable
    {
    public:
        concurrent_map() = default;
  
        template<class TKey,class TValue>
        bool set(TKey&& key, TValue&& value)
        {
            UNIQUE_LOCK_GURAD(lock_);
            auto iter = data_.find(key);
            if (iter != data_.end())
            {
                iter->second = std::forward<TValue>(value);
                return true;
            }
            else
            {
                auto ret = data_.emplace(key, value);
                return ret.second;
            }
        }

        bool try_get_value(const Key& key, Value& value) const
        {
            SHARED_LOCK_GURAD(lock_)
            return detail::try_get_value<Key, Value>::get(data_, key, value);
        }

        bool erase(const Key& key)
        {
            UNIQUE_LOCK_GURAD(lock_);
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
            UNIQUE_LOCK_GURAD(lock_);
            data_.clear();
        }

        size_t size() const
        {
            SHARED_LOCK_GURAD(lock_);
            return data_.size();
        }

        bool has(const Key& key) const
        {
            SHARED_LOCK_GURAD(lock_);
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

