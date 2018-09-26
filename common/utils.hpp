/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <tuple>
#include <utility>
#include <functional>
#include <memory>

namespace moon
{
    template<bool, typename TA, typename TB>
    struct if_then_else;

    template<typename TA, typename TB>
    struct if_then_else<true, TA, TB>
    {
        typedef TA type;
    };

    template<typename TA, typename TB>
    struct if_then_else<false, TA, TB>
    {
        typedef TB type;
    };

    using FuncBindTuple = std::tuple <decltype(std::placeholders::_1), decltype(std::placeholders::_2), decltype(std::placeholders::_3), decltype(std::placeholders::_4), decltype(std::placeholders::_5), decltype(std::placeholders::_6) >;

    namespace detail
    {
        template<typename TFunc, std::size_t... I>
        auto make_bind_imp(const TFunc& fn, std::index_sequence<I...>)->decltype(std::bind(fn, typename std::tuple_element<I, FuncBindTuple>::type()...))
        {
            return std::bind(fn, typename std::tuple_element<I, FuncBindTuple>::type()...);
        }

        template<typename TFunc, typename TObjectPointer, std::size_t... I>
        auto make_bind_imp(const TFunc& fn, TObjectPointer obj, std::index_sequence<I...>)->decltype(std::bind(fn, obj, typename std::tuple_element<I, FuncBindTuple>::type()...))
        {
            return std::bind(fn, obj, typename std::tuple_element<I, FuncBindTuple>::type()...);
        }
    }

    //std::function<R(TClass*,Args...)> make_bind(&some_member_func)
    template<typename R, typename TClass, typename... Args, typename Indices = std::make_index_sequence<sizeof...(Args) + 1>>
    auto make_bind(R(TClass::*fn)(Args...))->decltype(detail::make_bind_imp(fn, Indices()))
    {
        return detail::make_bind_imp(fn, Indices());
    }

    template<typename R, typename TClass, typename... Args, typename Indices = std::make_index_sequence<sizeof...(Args) + 1>>
    auto make_bind(R(TClass::*fn)(Args...) const)->decltype(detail::make_bind_imp(fn, Indices()))
    {
        return detail::make_bind_imp(fn, Indices());
    }

    template<typename R, typename TClass, typename... Args, typename TObjectPointer, typename Indices = std::make_index_sequence<sizeof...(Args)>>
    auto make_bind(R(TClass::*fn)(Args...), TObjectPointer obj)->decltype(detail::make_bind_imp(fn, obj, Indices()))
    {
        return detail::make_bind_imp(fn, obj, Indices());
    }

    template<typename R, typename TClass, typename... Args, typename TObjectPointer, typename Indices = std::make_index_sequence<sizeof...(Args)>>
    auto make_bind(R(TClass::*fn)(Args...) const, TObjectPointer obj)->decltype(detail::make_bind_imp(fn, obj, Indices()))
    {
        return detail::make_bind_imp(fn, obj, Indices());
    }

    template<typename Function>
    struct function_traits :public function_traits<decltype(&Function::operator())>
    {
    };

    template<typename ClassType, typename ReturnType, typename... Args>
    struct function_traits<ReturnType(ClassType::*)(Args...) const>
    {
        using function = std::function<ReturnType(Args...)>;
    };

    template<typename Function>
    typename function_traits<Function>::function to_function(Function& lambda)
    {
        return static_cast<typename function_traits<Function>::function>(lambda);
    };

    template <class T>
    inline bool bool_cast(const T& t)
    {
        return (t != 0);
    }

    template<typename T, std::size_t N>
    inline constexpr std::size_t array_szie(T(&)[N])
    {
        return N;
    }

    template<typename TMap>
    inline bool contains_key(const TMap& map, typename TMap::key_type key)
    {
        auto iter = map.find(key);
        return (iter != map.end());
    }

    template<typename TMap, typename TKey, typename TValue>
    inline bool try_get_value(const TMap& map, const TKey& key, TValue& value)
    {
        auto iter = map.find(key);
        if (iter != map.end())
        {
            value = iter->second;
            return true;
        }
        return false;
    }

}