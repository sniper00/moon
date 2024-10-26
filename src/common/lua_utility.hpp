#pragma once
#include "lua.hpp"
#include <cassert>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#define luaL_rawsetfield(L, tbindex, kname, valueexp) \
    lua_pushliteral(L, kname); \
    (valueexp); \
    lua_rawset(L, tbindex)

namespace moon {
struct lua_scope_pop {
    lua_State* lua;
    explicit lua_scope_pop(lua_State* L): lua(L) {}

    lua_scope_pop(const lua_scope_pop&) = delete;

    lua_scope_pop& operator=(const lua_scope_pop&) = delete;

    ~lua_scope_pop() {
        lua_pop(lua, 1);
    }
};

inline bool requiref(lua_State* L, const char* name, lua_CFunction f) {
    struct protect_call {
        static int require_cmodule(lua_State* L) {
            const char* name = luaL_checkstring(L, 1);
            lua_CFunction f = (lua_CFunction)lua_touserdata(L, 2);
            luaL_requiref(L, name, f, 0);
            return 0;
        }
    };

    lua_pushcfunction(L, protect_call::require_cmodule);
    lua_pushstring(L, name);
    lua_pushlightuserdata(L, (void*)f);
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        lua_pop(L, 1);
        return false;
    }
    return true;
}

template<typename T>
void table_to_vector(lua_State* L, int index, std::vector<T>& out) {
    auto size = lua_rawlen(L, index);
    out.reserve(size);
    for (size_t i = 1; i <= size; ++i) {
        lua_rawgeti(L, index, i);
        if constexpr (std::is_integral_v<T>) {
            out.emplace_back(static_cast<T>(luaL_checkinteger(L, -1)));
        } else if constexpr (std::is_floating_point_v<T>) {
            out.emplace_back(luaL_checknumber(L, -1));
        } else {
            size_t len;
            const char* data = luaL_checklstring(L, -1, &len);
            out.emplace_back(data, len);
        }
        lua_pop(L, 1);
    }
}

template<class LuaArrayView>
class lua_array_view_iterator {
    const LuaArrayView* lav_;
    size_t pos_;

public:
    using iterator_category = std::random_access_iterator_tag;
    using self_type = lua_array_view_iterator;
    using value_type = typename LuaArrayView::value_type;
    using pointer = value_type*;
    using reference = value_type&;
    using difference_type = std::ptrdiff_t;

    explicit lua_array_view_iterator(const LuaArrayView* lav, size_t pos): lav_(lav), pos_(pos) {}

    auto operator*() const {
        return lav_->get_lua_value(pos_ + 1);
    }

    self_type& operator++() {
        assert(pos_ != lav_->size());
        ++pos_;
        return *this;
    }

    self_type operator++(int) {
        assert(pos_ != lav_->size());
        auto old = pos_;
        pos_++;
        return self_type { lav_, old };
    }

    difference_type operator-(const self_type& _Right) const { // return difference of iterators
        return (pos_ - _Right.pos_);
    }

    bool operator!=(const self_type& other) const {
        return pos_ != other.pos_;
    }

    bool operator==(const self_type& other) const {
        return pos_ == other.pos_;
    }
};

template<typename ValueType>
class lua_array_view {
    int index_;
    lua_State* L_;
    size_t size_ = 0;

    auto get_lua_value(size_t pos) const {
        assert(pos <= size());
        lua_rawgeti(L_, index_, pos);
        lua_scope_pop lsp { L_ };
        if constexpr (std::is_integral_v<value_type>) {
            return static_cast<value_type>(luaL_checkinteger(L_, -1));
        } else if constexpr (std::is_floating_point_v<value_type>) {
            return static_cast<value_type>(luaL_checknumber(L_, -1));
        } else {
            size_t len;
            const char* data = luaL_checklstring(L_, -1, &len);
            return std::string { data, len };
        }
    }

public:
    using value_type = ValueType;

    using const_iterator = lua_array_view_iterator<lua_array_view>;

    friend const_iterator;

    lua_array_view(lua_State* L, int idx): index_(idx), L_(L) {
        size_ = lua_rawlen(L, idx);
    }

    size_t size() const {
        return size_;
    }

    bool empty() const {
        return size_ == 0;
    }

    lua_State* lua_state() const {
        return L_;
    }

    value_type operator[](size_t pos) const {
        return get_lua_value(pos + 1);
    }

    const_iterator begin() const {
        return const_iterator { this, 0 };
    }

    const_iterator end() const {
        return const_iterator { this, size() };
    }
};

struct state_deleter {
    void operator()(lua_State* L) const {
        lua_close(L);
    }
};

inline int traceback(lua_State* L) {
    if (const char* msg = lua_tostring(L, 1))
        luaL_traceback(L, L, msg, 1);
    else {
        lua_pushliteral(L, "(no error message)");
    }
    return 1;
}

//https://en.cppreference.com/w/cpp/language/if
template<class>
inline constexpr bool dependent_false_v = false;

template<typename Type>
Type lua_check(lua_State* L, int index) {
    using T = std::decay_t<Type>;
    if constexpr (std::is_same_v<T, std::string_view>) {
        size_t size;
        const char* sz = luaL_checklstring(L, index, &size);
        return std::string_view { sz, size };
    } else if constexpr (std::is_same_v<T, std::string>) {
        size_t size;
        const char* sz = luaL_checklstring(L, index, &size);
        return std::string { sz, size };
    } else if constexpr (std::is_same_v<T, bool>) {
        if (!lua_isboolean(L, index))
            luaL_typeerror(L, index, lua_typename(L, LUA_TBOOLEAN));
        return (bool)lua_toboolean(L, index);
    } else if constexpr (std::is_integral_v<T>) {
        auto v = luaL_checkinteger(L, index);
        luaL_argcheck(
            L,
            static_cast<lua_Integer>(static_cast<T>(v)) == v,
            index,
            "integer out-of-bounds"
        );
        return static_cast<T>(v);
    } else if constexpr (std::is_floating_point_v<T>) {
        return static_cast<T>(luaL_checknumber(L, index));
    } else {
        static_assert(dependent_false_v<T>, "unsupport type");
    }
}

template<typename Type>
inline Type
lua_opt_field(lua_State* L, int index, std::string_view key, const Type& def = Type {}) {
    if (index < 0) {
        index = lua_gettop(L) + index + 1;
    }

    assert(lua_type(L, index) == LUA_TTABLE);
    lua_pushlstring(L, key.data(), key.size());
    lua_scope_pop scope { L };
    if (lua_rawget(L, index) <= LUA_TNIL)
        return def;
    return lua_check<Type>(L, -1);
}

template<typename Type>
inline Type lua_check_field(lua_State* L, int index, std::string_view key) {
    if (index < 0) {
        index = lua_gettop(L) + index + 1;
    }

    luaL_checktype(L, index, LUA_TTABLE);
    lua_pushlstring(L, key.data(), key.size());
    lua_scope_pop scope { L };
    lua_rawget(L, index);
    return lua_check<Type>(L, -1);
}

inline bool luaL_optboolean(lua_State* L, int arg, bool def) {
    return luaL_opt(L, lua_check<bool>, arg, def);
}

inline std::string lua_tostring_unchecked(lua_State* L, int index) {
    int type = lua_type(L, index);
    switch (type) {
        case LUA_TNIL:
            return std::string { "nil" };
        case LUA_TNUMBER:
            return lua_isinteger(L, index) ? std::to_string(lua_tointeger(L, index))
                                           : std::to_string(lua_tonumber(L, index));
        case LUA_TBOOLEAN:
            return lua_toboolean(L, index) ? std::string { "true" } : std::string { "false" };
        case LUA_TSTRING: {
            size_t sz = 0;
            const char* str = lua_tolstring(L, index, &sz);
            return std::string { str, sz };
        }
        default:
            return std::string { "string type expected" };
    }
}
} // namespace moon
