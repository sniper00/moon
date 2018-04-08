/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include "platform_define.hpp"

#include <algorithm>
#include <cstdint>
#include <cassert>
#include <string>
#include <cstdarg>
#include <iomanip>

#include <thread>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <condition_variable>

#include <functional>

#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <array>
#include <forward_list>
#include <queue>

#include <memory>

#include "exception.hpp"

#include "string_ref.hpp"

namespace moon
{
    using string_view_t = string_ref_t;
    using wstring_view_t = wstring_ref_t;
    constexpr string_view_t operator "" _sv(const char *_Str, size_t _Len) noexcept
    {	// construct string_view from [_Str, _Str + _Len)
        return (string_view_t(_Str, _Len));
    }
}

#undef min
#undef max

#ifndef INLINE
#define INLINE		inline
#endif

#define BREAK_IF(x) if(x) break;
#define SAFE_DELETE(x) if(nullptr != x) {delete x; x = nullptr;}
#define SAFE_DELETE_ARRAY(x) if(nullptr != x) {delete []x; x = nullptr;}

#define VA_ARGS_NUM(...) std::tuple_size<decltype(std::make_tuple(__VA_ARGS__))>::value

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

#define thread_sleep(x)  std::this_thread::sleep_for(std::chrono::milliseconds(x));

#define SHARED_LOCK_GURAD(lock) std::shared_lock<decltype(lock)> lk(lock);
#define UNIQUE_LOCK_GURAD(lock) std::unique_lock<decltype(lock)> lk(lock);


