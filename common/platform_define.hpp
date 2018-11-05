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

#if (TARGET_PLATFORM == PLATFORM_WINDOWS)
#ifndef __MINGW32__
#pragma warning (disable:4127) 
#endif 
#endif  // PLATFORM_WIN32

#if TARGET_PLATFORM == PLATFORM_WINDOWS
#include <WinSock2.h>
#include <process.h> //  _get_pid support
#define strnicmp	_strnicmp
#ifdef  _WIN64
typedef __int64    ssize_t;
#else
typedef _W64 int   ssize_t;
#endif
#else
#if __cplusplus < 201103
#error  "your compiler does not support C++11"
#endif
#endif

#if TARGET_PLATFORM == PLATFORM_LINUX
#include <sys/syscall.h>
#include <unistd.h>
#endif

#if defined(_USRDLL)
#define MOON_EXPORT __declspec(dllexport)
#else /* use as DLL library */
#define MOON_EXPORT
#endif

#ifndef __has_feature       // Clang - feature checking macros.
#define __has_feature(x) 0  // Compatibility with non-clang compilers.
#endif

#include<cstdint>

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