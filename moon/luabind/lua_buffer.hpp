#pragma once
#include "sol.hpp"
#include "message.hpp"
#include "lua_serialize.hpp"
namespace sol
{
    template <>
    struct lua_size<moon::buffer_ptr_t> : std::integral_constant<int, 1> {};

    namespace stack {
        template <>
        struct checker<moon::buffer_ptr_t> {
            template <typename Handler>
            static bool check(lua_State* L, int index, Handler&& handler, record& tracking) {
                tracking.use(1);
                type t = type_of(L, index);
                if (t == type::nil || t == type::userdata || t == type::lightuserdata || t == type::string)
                {
                    return true;
                }
                else
                {
                    handler(L, index, type::lightuserdata, t, "expected a nil or a userdata(buffer*) or a string");
                    return false;
                }
            }
        };

        template <>
        struct getter<moon::buffer_ptr_t> {
            static moon::buffer_ptr_t get(lua_State* L, int index, record& tracking) {
                tracking.use(1);
                type t = type_of(L, index);
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
                    buf->write_back(str, 0, len);
                    return buf;
                }
                case sol::type::userdata:
                case sol::type::lightuserdata:
                {
                    moon::buffer* p = static_cast<moon::buffer*>(lua_touserdata(L, index));
                    return moon::buffer_ptr_t(p);
                }
                default:
                    break;
                }
                luaL_error(L, "get buffer only support string or void*(buffer*)");
                return nullptr;
            }
        };

        template <>
        struct pusher<moon::buffer_ptr_t> {
            static int push(lua_State* L, const moon::buffer_ptr_t& buf) {
                if (nullptr == buf)
                {
                    return stack::push(L, lua_nil);
                }
                return stack::push(L, (const char*)buf->data(), buf->size());
            }
        };
    }
}

