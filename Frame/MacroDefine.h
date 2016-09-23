/****************************************************************************

Git <https://github.com/sniper00/moon_net>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include "PlatformConfig.h"

#include <iostream>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <string>

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

#include <functional>

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <queue>
#include <array>

#include <memory>

#ifndef INLINE
#define INLINE		inline
#endif

#define BREAK_IF(x) if(x) break;
#define SAFE_DELETE(x) if(nullptr != x) {delete x; x = nullptr;}
#define SAFE_DELETE_ARRAY(x) if(nullptr != x) {delete []x; x = nullptr;}


#if TARGET_PLATFORM == PLATFORM_WINDOWS
#ifdef EXPORT_MOON_NET
#define MOON_DLL	__declspec(dllexport)
#else
#define MOON_DLL
#endif
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

#undef min
#undef max

using gurad_lock = std::unique_lock<std::mutex>;

#define thread_sleep(x)  std::this_thread::sleep_for(std::chrono::milliseconds(x));

#define PROPERTY_READONLY(varType,varName,funName)\
protected: varType varName =varType();\
public: const varType& Get##funName(void) const { return varName; }\
protected:void Set##funName(const varType& v) { varName = v; }

#define PROPERTY_READWRITE(varType,varName,funName)\
protected: varType varName =varType();\
public: const varType& Get##funName(void) const { return varName; }\
	void Set##funName(const varType& v) { varName = v; }

#define DECLARE_SHARED_PTR(classname)\
class classname;\
using classname##Ptr = std::shared_ptr<classname>;

#define DECLARE_WEAK_PTR(classname)\
class classname;\
using classname##WPtr = std::weak_ptr<classname>;


#ifdef DEBUG
#define Assert(cnd,msg) assert((cnd)&&(msg))
#else
#define Assert(cnd,msg) {if(!(cnd)) throw std::runtime_error(msg);}
#endif // DEBUG

#define CHECKWARNING(cnd,msg)  {if(!(cnd)) printf(msg);printf("\r\n");}


template <class T>
inline bool bool_cast(const T& t)
{
	return (t != 0);
}

template<typename T, std::size_t N>
constexpr std::size_t array_szie(T(&)[N])
{
	return N;
}

template<typename TMap>
bool contains_key(const TMap& map, typename TMap::key_type key)
{
	auto iter = map.find(key);
	return (iter != map.end());
}


using ModuleID = uint32_t;
using SessionID = uint32_t;

using moduleid_t = uint32_t;
using sessionid_t = uint32_t;




