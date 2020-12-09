#include "lua.hpp"
#include "message.hpp"

using namespace moon;

static int message_decode(lua_State* L)
{
    message* m = (message*)lua_touserdata(L, 1);
    if (nullptr == m)
    {
        return luaL_error(L, "message info param 1 need userdata");
    }
    size_t len = 0;
    const char* sz = luaL_checklstring(L, 2, &len);
    int top = lua_gettop(L);
    for (size_t i = 0; i < len; ++i)
    {
        switch (sz[i])
        {
        case 'S':
            lua_pushinteger(L, m->sender());
            break;
        case 'R':
            lua_pushinteger(L, m->receiver());
            break;
        case 'E':
            lua_pushinteger(L, m->sessionid());
            break;
        case 'H':
        {
            std::string_view header = m->header();
            if (!header.empty())
            {
                lua_pushlstring(L, header.data(), header.size());
            }
            else
            {
                lua_pushnil(L);
            }
            break;
        }
        case 'Z':
        {
            std::string_view str = m->bytes();
            if (!str.empty())
            {
                lua_pushlstring(L, str.data(), str.size());
            }
            else
            {
                lua_pushnil(L);
            }
            break;
        }
        case 'N':
        {
            lua_pushinteger(L, m->size());
            break;
        }
        case 'B':
        {
            lua_pushlightuserdata(L, m->get_buffer());
            break;
        }
        case 'C':
        {
            buffer* buf = m->get_buffer();
            if (nullptr == buf)
            {
                lua_pushnil(L);
                lua_pushnil(L);
            }
            else
            {
                lua_pushlightuserdata(L, m->get_buffer()->data());
                lua_pushinteger(L, m->get_buffer()->size());
            }
            break;
        }
        default:
            return luaL_error(L, "message decode get unknown cmd %s", sz);
        }
    }
    return lua_gettop(L) - top;
}

static int message_clone(lua_State* L)
{
    message* m = (message*)lua_touserdata(L, 1);
    if (nullptr == m)
    {
        return luaL_error(L, "message clone param 1 need userdata");
    }
    message* nm = new message((const buffer_ptr_t&)*m);
    nm->set_broadcast(m->broadcast());
    nm->set_header(m->header());
    nm->set_receiver(m->receiver());
    nm->set_sender(m->sender());
    nm->set_sessionid(m->sessionid());
    nm->set_type(m->type());
    lua_pushlightuserdata(L, nm);
    return 1;
}

static int message_release(lua_State* L)
{
    message* m = (message*)lua_touserdata(L, 1);
    if (nullptr == m)
    {
        return luaL_error(L, "message release param 1 need userdata");
    }
    delete m;
    return 0;
}

static int message_redirect(lua_State* L)
{
    int top = lua_gettop(L);
    message* m = (message*)lua_touserdata(L, 1);
    if (nullptr == m)
    {
        return luaL_error(L, "message clone param 1 need userdata");
    }
    size_t len = 0;
    const char* sz = luaL_checklstring(L, 2, &len);
    m->set_header(std::string_view{ sz, len });
    m->set_receiver((uint32_t)luaL_checkinteger(L, 3));
    m->set_type((uint8_t)luaL_checkinteger(L, 4));
    if (top > 4)
    {
        m->set_sender((uint32_t)luaL_checkinteger(L, 5));
        m->set_sessionid((int32_t)luaL_checkinteger(L, 6));
    }
    return 0;
}

extern "C"
{
    int LUAMOD_API luaopen_message(lua_State* L)
    {
        luaL_Reg l[] = {
            { "decode", message_decode},
            { "clone", message_clone },
            { "release", message_release },
            { "redirect", message_redirect},
            {NULL,NULL}
        };
        luaL_checkversion(L);
        luaL_newlib(L, l);
        return 1;
    }
}

