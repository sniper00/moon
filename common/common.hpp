#pragma once

#define PLATFORM_UNKNOWN 0
#define PLATFORM_WINDOWS 1
#define PLATFORM_LINUX 2
#define PLATFORM_MAC 3

#define TARGET_PLATFORM PLATFORM_UNKNOWN

// mac
#if defined(__APPLE__) && (defined(__GNUC__) || defined(__xlC__) || defined(__xlc__))
    #undef TARGET_PLATFORM
    #define TARGET_PLATFORM PLATFORM_MAC
#endif

// win32
#if !defined(SAG_COM) \
    && (defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__))
    #if defined(WINCE) || defined(_WIN32_WCE)
    //win ce
    #else
        #undef TARGET_PLATFORM
        #define TARGET_PLATFORM PLATFORM_WINDOWS
    #endif
#endif

#if !defined(SAG_COM) && (defined(WIN64) || defined(_WIN64) || defined(__WIN64__))
    #undef TARGET_PLATFORM
    #define TARGET_PLATFORM PLATFORM_WINDOWS
#endif

// linux
#if defined(__linux__) || defined(__linux)
    #undef TARGET_PLATFORM
    #define TARGET_PLATFORM PLATFORM_LINUX
#endif

//////////////////////////////////////////////////////////////////////////
// post configure
//////////////////////////////////////////////////////////////////////////

// check user set platform
#if !TARGET_PLATFORM
    #error "Cannot recognize the target platform; are you targeting an unsupported platform?"
#endif

#if TARGET_PLATFORM == PLATFORM_WINDOWS
    #include <WinSock2.h>
    #include <process.h> //  _get_pid support
    #include <stdarg.h>
    #include <stdio.h>
    #define strnicmp _strnicmp

inline int moon_vsnprintf(char* buffer, size_t count, const char* format, va_list argptr) {
    return vsnprintf_s(buffer, count, _TRUNCATE, format, argptr);
}

    #ifdef _WIN64
typedef __int64 ssize_t;
    #else
typedef _W64 int ssize_t;
    #endif
#else
    #include <sys/syscall.h>
    #include <unistd.h>
    #define moon_vsnprintf vsnprintf
#endif

#ifndef __has_feature // Clang - feature checking macros.
    #define __has_feature(x) 0 // Compatibility with non-clang compilers.
#endif

#include <algorithm>
#include <cassert>
#include <cctype>
#include <charconv>
#include <cinttypes>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <thread>

#include <array>
#include <deque>
#include <forward_list>
#include <map>
#include <queue>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include <memory>
#include <utility>

#include "exception.hpp"

using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;

#undef min
#undef max

#define VA_ARGS_NUM(...) std::tuple_size<decltype(std::make_tuple(__VA_ARGS__))>::value

#define thread_sleep(x) std::this_thread::sleep_for(std::chrono::milliseconds(x));

namespace moon {
inline size_t _thread_id() {
#ifdef _WIN32
    return static_cast<size_t>(::GetCurrentThreadId());
#elif __linux__
    #if defined(__ANDROID__) && defined(__ANDROID_API__) && (__ANDROID_API__ < 21)
        #define SYS_gettid __NR_gettid
    #endif
    return static_cast<size_t>(syscall(SYS_gettid));
#elif __FreeBSD__
    long tid;
    thr_self(&tid);
    return static_cast<size_t>(tid);
#else //Default to standard C++11 (OSX and other Unix)
    return static_cast<size_t>(std::hash<std::thread::id>()(std::this_thread::get_id())) % 10000000;
#endif
}

//Return current thread id as size_t (from thread local storage)
inline size_t thread_id() {
#if defined(_MSC_VER) && (_MSC_VER < 1900) || defined(__clang__) && !__has_feature(cxx_thread_local)
    return _thread_id();
#else
    static thread_local const size_t tid = _thread_id();
    return tid;
#endif
}

inline int pid() {
#if (TARGET_PLATFORM == PLATFORM_WINDOWS)
    return ::_getpid();
#else
    return static_cast<int>(::getpid());
#endif
}

using FuncBindTuple = std::tuple<
    decltype(std::placeholders::_1),
    decltype(std::placeholders::_2),
    decltype(std::placeholders::_3),
    decltype(std::placeholders::_4),
    decltype(std::placeholders::_5),
    decltype(std::placeholders::_6)>;

namespace detail {
    template<typename TFunc, std::size_t... I>
    auto make_bind_imp(const TFunc& fn, std::index_sequence<I...>)
        -> decltype(std::bind(fn, typename std::tuple_element<I, FuncBindTuple>::type()...)) {
        return std::bind(fn, typename std::tuple_element<I, FuncBindTuple>::type()...);
    }

    template<typename TFunc, typename TObjectPointer, std::size_t... I>
    auto make_bind_imp(const TFunc& fn, TObjectPointer obj, std::index_sequence<I...>)
        -> decltype(std::bind(fn, obj, typename std::tuple_element<I, FuncBindTuple>::type()...)) {
        return std::bind(fn, obj, typename std::tuple_element<I, FuncBindTuple>::type()...);
    }
} // namespace detail

//std::function<R(TClass*,Args...)> make_bind(&some_member_func)
template<
    typename R,
    typename TClass,
    typename... Args,
    typename Indices = std::make_index_sequence<sizeof...(Args) + 1>>
auto make_bind(R (TClass::*fn)(Args...)) -> decltype(detail::make_bind_imp(fn, Indices())) {
    return detail::make_bind_imp(fn, Indices());
}

template<
    typename R,
    typename TClass,
    typename... Args,
    typename Indices = std::make_index_sequence<sizeof...(Args) + 1>>
auto make_bind(R (TClass::*fn)(Args...) const) -> decltype(detail::make_bind_imp(fn, Indices())) {
    return detail::make_bind_imp(fn, Indices());
}

template<
    typename R,
    typename TClass,
    typename... Args,
    typename TObjectPointer,
    typename Indices = std::make_index_sequence<sizeof...(Args)>>
auto make_bind(R (TClass::*fn)(Args...), TObjectPointer obj)
    -> decltype(detail::make_bind_imp(fn, obj, Indices())) {
    return detail::make_bind_imp(fn, obj, Indices());
}

template<
    typename R,
    typename TClass,
    typename... Args,
    typename TObjectPointer,
    typename Indices = std::make_index_sequence<sizeof...(Args)>>
auto make_bind(R (TClass::*fn)(Args...) const, TObjectPointer obj)
    -> decltype(detail::make_bind_imp(fn, obj, Indices())) {
    return detail::make_bind_imp(fn, obj, Indices());
}

template<typename Function>
struct function_traits: public function_traits<decltype(&Function::operator())> {};

template<typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType (ClassType::*)(Args...) const> {
    using function = std::function<ReturnType(Args...)>;
};

template<typename Function>
typename function_traits<Function>::function to_function(Function& lambda) {
    return static_cast<typename function_traits<Function>::function>(lambda);
};

template<class T>
inline bool bool_cast(const T& t) {
    return (t != 0);
}

template<typename T, std::size_t N>
inline constexpr std::size_t array_szie(T (&)[N]) {
    return N;
}

template<typename TMap>
inline bool contains_key(const TMap& map, typename TMap::key_type key) {
    return (map.find(key) != map.end());
}

template<typename TMap, typename TKey, typename TValue>
inline bool try_get_value(const TMap& map, const TKey& key, TValue& value) {
    auto iter = map.find(key);
    if (iter != map.end()) {
        value = iter->second;
        return true;
    }
    return false;
}

template<typename Enum>
struct enum_enable_bitmask_operators {
    static constexpr bool enable = false;
};

template<typename Enum>
inline typename std::enable_if_t<enum_enable_bitmask_operators<Enum>::enable, Enum>
operator|(Enum a, Enum b) {
    return static_cast<Enum>(
        static_cast<std::underlying_type_t<Enum>>(a) | static_cast<std::underlying_type_t<Enum>>(b)
    );
}

template<typename Enum>
inline typename std::enable_if_t<enum_enable_bitmask_operators<Enum>::enable, Enum>
operator&(Enum a, Enum b) {
    return static_cast<Enum>(
        static_cast<std::underlying_type_t<Enum>>(a) & static_cast<std::underlying_type_t<Enum>>(b)
    );
}

template<
    typename Enum,
    typename std::enable_if_t<enum_enable_bitmask_operators<Enum>::enable, int> = 0>
inline bool enum_has_any_bitmask(Enum v, Enum contains) {
    using under_type = typename std::underlying_type<Enum>::type;
    return (static_cast<under_type>(v) & static_cast<under_type>(contains)) != 0;
}
} // namespace moon