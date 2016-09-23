/****************************************************************************

Git <https://github.com/sniper00/moon_net>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <tuple>
#include <utility>
#include <functional>
#include <memory>

namespace moon
{
	template<size_t... >
	struct integer_sequence {};

	template<size_t N, size_t... Indices>
	struct make_integer_sequence
	{
		using type = typename make_integer_sequence<N - 1, N - 1, Indices... >::type;
	};

	template<size_t... Indices>
	struct make_integer_sequence<0, Indices... >
	{
		using type = integer_sequence<Indices... >;
	};

	template<size_t size>
	using make_index_sequence = typename make_integer_sequence<size>::type;

	using FuncBindTuple = std::tuple <decltype(std::placeholders::_1), decltype(std::placeholders::_2), decltype(std::placeholders::_3), decltype(std::placeholders::_4), decltype(std::placeholders::_5), decltype(std::placeholders::_6) >;

	namespace detail
	{
		template<typename TFunc, std::size_t... I>
		auto make_bind_imp(const TFunc& fn, integer_sequence<I...>)->decltype(std::bind(fn, typename std::tuple_element<I, FuncBindTuple>::type()...))
		{
			return std::bind(fn, typename std::tuple_element<I, FuncBindTuple>::type()...);
		}

		template<typename TFunc, typename TObjectPointer, std::size_t... I>
		auto make_bind_imp(const TFunc& fn, TObjectPointer obj, integer_sequence<I...>)->decltype(std::bind(fn, obj, typename std::tuple_element<I, FuncBindTuple>::type()...))
		{
			return std::bind(fn, obj, typename std::tuple_element<I, FuncBindTuple>::type()...);
		}
	}

	//std::bind 特殊使用，有时绑定成员函数的时候暂时不需要传入对象
	//std::function<R(TClass*,Args...)> make_bind(&some_member_func)
	template<typename R, typename TClass, typename... Args, typename Indices = make_index_sequence<sizeof...(Args)+1>>
	auto make_bind(R(TClass::*fn)(Args...))->decltype(detail::make_bind_imp(fn, Indices()))
	{
		return detail::make_bind_imp(fn, Indices());
	}

	template<typename R, typename TClass, typename... Args, typename Indices = make_index_sequence<sizeof...(Args)+1>>
	auto make_bind(R(TClass::*fn)(Args...) const)->decltype(detail::make_bind_imp(fn, Indices()))
	{
		return detail::make_bind_imp(fn, Indices());
	}


	template<typename R, typename TClass, typename... Args, typename Indices = make_index_sequence<sizeof...(Args)>>
	auto make_bind(R(TClass::*fn)(Args...), TClass* obj)->decltype(detail::make_bind_imp(fn, obj, Indices()))
	{
		return detail::make_bind_imp(fn, obj, Indices());
	}

	template<typename R, typename TClass, typename... Args, typename Indices = make_index_sequence<sizeof...(Args)>>
	auto make_bind(R(TClass::*fn)(Args...) const, const TClass* obj)->decltype(detail::make_bind_imp(fn, obj, Indices()))
	{
		return detail::make_bind_imp(fn, obj, Indices());
	}

	//shared_ptr support
	template<typename R, typename TClass, typename... Args, typename Indices = make_index_sequence<sizeof...(Args)>>
	auto make_bind(R(TClass::*fn)(Args...), const std::shared_ptr<TClass>& obj)->decltype(detail::make_bind_imp(fn, obj, Indices()))
	{
		return detail::make_bind_imp(fn, obj, Indices());
	}

	//shared_ptr support
	template<typename R, typename TClass, typename... Args, typename Indices = make_index_sequence<sizeof...(Args)>>
	auto make_bind(R(TClass::*fn)(Args...) const, const std::shared_ptr<TClass>& obj)->decltype(detail::make_bind_imp(fn, obj, Indices()))
	{
		return detail::make_bind_imp(fn, obj, Indices());
	}
}