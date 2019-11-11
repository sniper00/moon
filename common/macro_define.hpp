#pragma once
#include "platform_define.hpp"
#include <cinttypes>
#include <string_view>
#include <algorithm>
#include <cstdint>
#include <cassert>
#include <string>
#include <cstdarg>
#include <iomanip>
#include <fstream>

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

using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;

namespace moon
{
    using string_view_t = std::string_view;
    using wstring_view_t = std::wstring_view;
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
    enum  class state
    {
        unknown,
        init,
        ready,
        stopping,
        exited
    };
}


