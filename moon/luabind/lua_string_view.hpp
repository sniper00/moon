#pragma once
namespace sol
{
    template <>
    struct lua_size<moon::string_view_t> : std::integral_constant<int, 2> {};

    namespace stack {
        template <>
        struct checker<moon::string_view_t> {
            template <typename Handler>
            static bool check(lua_State* L, int index, Handler&& handler, record& tracking) {
                tracking.use(1);
                type t = type_of(L, index);
                if (t == type::nil || t == type::string)
                {
                    return true;
                }
                else
                {
                    handler(L, index, type::string, t);
                    return false;
                }
            }
        };

        template <>
        struct getter<moon::string_view_t> {
            static moon::string_view_t get(lua_State* L, int index, record& tracking) {
                tracking.use(1);
                std::size_t len;
                auto str = lua_tolstring(L, index, &len);
                return moon::string_view_t{ str,len };
            }
        };

        template <>
        struct pusher<moon::string_view_t> {
            static int push(lua_State* L, moon::string_view_t s) {
                return stack::push(L,s.data(), s.size());
            }
        };
    }
}

