#pragma once
#include "sol.hpp"
#include "message.hpp"
#include "lua_serialize.hpp"

namespace moon
{
    template <typename Handler>
    inline static bool sol_lua_check(sol::types<buffer_ptr_t>, lua_State* L, int index, Handler&& handler, sol::stack::record& tracking) {
        sol::type t = sol::type_of(L, index);
        if (t == sol::type::lua_nil || t == sol::type::lightuserdata || t == sol::type::string)
        {
            tracking.use(1);
            return true;
        }
        else
        {
            handler(L, index, sol::type::lightuserdata, t, "expected nil or a  lightuserdata(buffer*) or a string");
            return false;
        }
    }

    inline static buffer_ptr_t sol_lua_get(sol::types<buffer_ptr_t>, lua_State* L, int index, sol::stack::record& tracking) {
        tracking.use(1);
        sol::type t = sol::type_of(L, index);
        switch (t)
        {
        case sol::type::lua_nil:
        {
            return nullptr;
        }
        case sol::type::string:
        {
            std::size_t len;
            auto str = lua_tolstring(L, index, &len);
            auto buf = moon::message::create_buffer(len);
            buf->write_back(str, len);
            return buf;
        }
        case sol::type::lightuserdata:
        {
            moon::buffer* p = static_cast<moon::buffer*>(lua_touserdata(L, index));
            return moon::buffer_ptr_t(p);
        }
        default:
            break;
        }
        luaL_error(L, "expected nil or a  lightuserdata(buffer*) or a string");
        return nullptr;
    }

    inline static int sol_lua_push(sol::types<buffer_ptr_t>, lua_State* L, const moon::buffer_ptr_t& buf) {
        if (nullptr == buf)
        {
            return sol::stack::push(L, sol::lua_nil);
        }
        return sol::stack::push(L, (const char*)buf->data(), buf->size());
    }

}
