#pragma once

#define PLATFORM_UNKNOWN		0
#define PLATFORM_WINDOWS			1
#define PLATFORM_LINUX					2
#define PLATFORM_MAC					3

#define TARGET_PLATFORM				PLATFORM_UNKNOWN

// mac
#if defined(__APPLE__) && (defined(__GNUC__) || defined(__xlC__) || defined(__xlc__))  
#undef  TARGET_PLATFORM
#define TARGET_PLATFORM         PLATFORM_MAC
#endif

// win32
#if !defined(SAG_COM) && (defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__))  
#if defined(WINCE) || defined(_WIN32_WCE)
//win ce
#else
#undef  TARGET_PLATFORM
#define TARGET_PLATFORM         PLATFORM_WINDOWS
#endif
#endif

#if !defined(SAG_COM) && (defined(WIN64) || defined(_WIN64) || defined(__WIN64__))
#undef  TARGET_PLATFORM
#define TARGET_PLATFORM         PLATFORM_WINDOWS
#endif

// linux
#if defined(__linux__) || defined(__linux) 
#undef  TARGET_PLATFORM
#define TARGET_PLATFORM         PLATFORM_LINUX
#endif

//////////////////////////////////////////////////////////////////////////
// post configure
//////////////////////////////////////////////////////////////////////////

// check user set platform
#if ! TARGET_PLATFORM
#error  "Cannot recognize the target platform; are you targeting an unsupported platform?"
#endif 

#if TARGET_PLATFORM == PLATFORM_WINDOWS
#include <WinSock2.h>
#include <process.h> //  _get_pid support
#include <stdio.h>
#include <stdarg.h>
#define strnicmp	_strnicmp

inline int moon_vsnprintf(char* buffer,
    size_t count,
    const char* format,
    va_list argptr)
{
    return vsnprintf_s(buffer, count, _TRUNCATE, format, argptr);
}

#ifdef  _WIN64
typedef __int64    ssize_t;
#else
typedef _W64 int   ssize_t;
#endif
#else
#include <sys/syscall.h>
#include <unistd.h>
#define moon_vsnprintf vsnprintf 
#endif

#ifndef __has_feature       // Clang - feature checking macros.
#define __has_feature(x) 0  // Compatibility with non-clang compilers.
#endif

#include <cstdint>
#include <cinttypes>
#include <algorithm>
#include <cstdint>
#include <cassert>
#include <cstdarg>
#include <iomanip>
#include <fstream>
#include <functional>
#include <locale>
#include <cmath>
#include <sstream>
#include <cctype>
#include <charconv>
#include <iostream>

#include <thread>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <condition_variable>

#include <vector>
#include <array>
#include <map>
#include <string_view>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <array>
#include <forward_list>
#include <queue>

#include <memory>

#include "exception.hpp"

using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;

#undef min
#undef max

#define VA_ARGS_NUM(...) std::tuple_size<decltype(std::make_tuple(__VA_ARGS__))>::value

#define DECLARE_SHARED_PTR(classname)\
class classname;\
using classname##_ptr_t = std::shared_ptr<classname>;

#define DECLARE_UNIQUE_PTR(classname)\
class classname;\
using classname##_ptr_t = std::unique_ptr<classname>;

#define DECLARE_WEAK_PTR(classname)\
class classname;\
using classname##_wptr_t = std::weak_ptr<classname>;

#define thread_sleep(x)  std::this_thread::sleep_for(std::chrono::milliseconds(x));

namespace moon
{
    inline size_t _thread_id()
    {
#ifdef _WIN32
        return  static_cast<size_t>(::GetCurrentThreadId());
#elif __linux__
# if defined(__ANDROID__) && defined(__ANDROID_API__) && (__ANDROID_API__ < 21)
#  define SYS_gettid __NR_gettid
# endif
        return  static_cast<size_t>(syscall(SYS_gettid));
#elif __FreeBSD__
        long tid;
        thr_self(&tid);
        return static_cast<size_t>(tid);
#else //Default to standard C++11 (OSX and other Unix)
        return static_cast<size_t>(std::hash<std::thread::id>()(std::this_thread::get_id()));
#endif
    }

    //Return current thread id as size_t (from thread local storage)
    inline size_t thread_id()
    {
#if defined(_MSC_VER) && (_MSC_VER < 1900) || defined(__clang__) && !__has_feature(cxx_thread_local)
        return _thread_id();
#else
        static thread_local const size_t tid = _thread_id();
        return tid;
#endif
    }

    inline int pid()
    {
#if (TARGET_PLATFORM == PLATFORM_WINDOWS)
        return ::_getpid();
#else
        return static_cast<int>(::getpid());
#endif
    }
}