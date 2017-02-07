/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include "platform_define.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <string>
#include <cstdarg>
#include <iomanip> 

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

#include <functional>

#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <array>

#include <memory>

#ifndef INLINE
#define INLINE		inline
#endif

#define BREAK_IF(x) if(x) break;
#define SAFE_DELETE(x) if(nullptr != x) {delete x; x = nullptr;}
#define SAFE_DELETE_ARRAY(x) if(nullptr != x) {delete []x; x = nullptr;}

#define VA_ARGS_NUM(...) std::tuple_size<decltype(std::make_tuple(__VA_ARGS__))>::value

#if TARGET_PLATFORM == PLATFORM_WINDOWS
#include <WinSock2.h>
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

#if defined(_USRDLL)
#define MOON_EXPORT __declspec(dllexport)
#else /* use a DLL library */
#define MOON_EXPORT
#endif

#undef min
#undef max

#define PROPERTY_READONLY(varType,varName,funName)\
protected: varType varName =varType();\
public: const varType& get_##funName(void) const { return varName; }\
protected:void set_##funName(const varType& v) { varName = v; }

#define PROPERTY_READWRITE(varType,varName,funName)\
protected: varType varName =varType();\
public: const varType& get_##funName(void) const { return varName; }\
	void set_##funName(const varType& v) { varName = v; }

#define DECLARE_SHARED_PTR(classname)\
class classname;\
using classname##_ptr_t = std::shared_ptr<classname>;

#define DECLARE_WEAK_PTR(classname)\
class classname;\
using classname##_wptr_t = std::weak_ptr<classname>;

#define Assert(cnd,msg) {if(!(cnd)) throw std::runtime_error(msg);}

using gurad_lock = std::unique_lock<std::mutex>;

#define thread_sleep(x)  std::this_thread::sleep_for(std::chrono::milliseconds(x));

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








