#include "kcp/ikcp.h"
#include "lua.hpp"
#include <string>
#include <queue>
#include "common/buffer.hpp"
#include "common/time.hpp"

struct box
{
    int32_t session = 0;
    size_t readn = 0;
    std::queue<std::string> wqueue;
    moon::buffer rbuf = moon::buffer{8192,0};
};

static int udp_output(const char* buf, int len, ikcpcb*, void* user) {
    box* ud = (box*)user;
    ud->wqueue.emplace(buf, (size_t)len);
    return 0;
}

static int lua_ikcp_create(lua_State* L) {
    IUINT32 conv = (IUINT32)luaL_checkinteger(L, 1);
    ikcpcb* kcp = ikcp_create(conv, new box{});
    ikcp_wndsize(kcp, 1024, 1024);
    ikcp_nodelay(kcp, 1, 10, 2, 1);
    kcp->rx_minrto = 10;
    kcp->stream = 1;
    ikcp_setoutput(kcp, udp_output);
    lua_pushlightuserdata(L, kcp);
    return 1;
}

static int lua_ikcp_release(lua_State* L) {
    ikcpcb* kcp = (ikcpcb*)(lua_touserdata(L, 1));
    if (kcp == NULL) { return luaL_error(L, "null kcp pointer"); }
    box* ud = (box*)kcp->user;
    delete ud;
    kcp->user = nullptr;
    ikcp_release(kcp);
    return 0;
}

inline static IUINT32 current()
{
    return  ((IUINT32)(moon::time::clock() * 1000));
}

static int lua_ikcp_update(lua_State* L) {
    ikcpcb* kcp = (ikcpcb*)(lua_touserdata(L, 1));
    if (kcp == NULL) { return luaL_error(L, "null kcp pointer"); }
    ikcp_update(kcp, current());
    IUINT32 millisec = ikcp_check(kcp, current());
    lua_pushinteger(L, millisec);
    return 1;
}

static int lua_ikcp_input(lua_State* L) {
    ikcpcb* kcp = (ikcpcb*)(lua_touserdata(L, 1));
    if (kcp == NULL) { return luaL_error(L, "null kcp pointer"); }
    size_t size = 0;
    const char* str = nullptr;
    if (lua_type(L, 2) == LUA_TSTRING) {
        str = luaL_checklstring(L, 2, &size);
    }
    else {
        str = reinterpret_cast<const char*>(lua_touserdata(L, 2));
        size = luaL_checkinteger(L, 3);
    }
    if (size == 0 || nullptr == str)
        return 0;
    auto res = ikcp_input(kcp, str, (int)size);
    lua_pushinteger(L, res);
    return 1;
}

static int lua_ikcp_output(lua_State* L) {
    ikcpcb* kcp = (ikcpcb*)(lua_touserdata(L, 1));
    if (kcp == NULL) { return luaL_error(L, "null kcp pointer"); }
    box* ud = (box*)kcp->user;
    if (ud->wqueue.empty())
        return 0;
    auto v = ud->wqueue.front();
    ud->wqueue.pop();
    lua_pushlstring(L, v.data(), v.size());
    return 1;
}

static int lua_ikcp_send(lua_State* L) {
    ikcpcb* kcp = (ikcpcb*)(lua_touserdata(L, 1));
    if (kcp == NULL) { return luaL_error(L, "null kcp pointer"); }
    size_t size = 0;
    const char* str = nullptr;
    if (lua_type(L, 2) == LUA_TSTRING) {
        str = luaL_checklstring(L, 2, &size);
    }
    else {
        str = reinterpret_cast<const char*>(lua_touserdata(L, 2));
        size = luaL_checkinteger(L, 3);
    }
    if (size == 0 || nullptr == str)
        return 0;
    auto res = ikcp_send(kcp, str, (int)size);
    lua_pushinteger(L, res);
    return 1;
}

static int lua_ikcp_poll_read(lua_State* L) {
    ikcpcb* kcp = (ikcpcb*)(lua_touserdata(L, 1));
    if (kcp == NULL) { return luaL_error(L, "null kcp pointer"); }
    box* ud = (box*)kcp->user;
    auto space = ud->rbuf.prepare(2048);
    int len = ikcp_recv(kcp, space.first, static_cast<int>(space.second));
    if (len > 0)
        ud->rbuf.commit(len);

    if (ud->session != 0 && ud->readn <= ud->rbuf.size())
    {
        lua_pushinteger(L, ud->session);
        lua_pushlstring(L, ud->rbuf.data(), ud->readn);
        ud->rbuf.consume(ud->readn);
        ud->session = 0;
        ud->readn = 0;
        return 2;
    }
    return 0;
}

static int lua_ikcp_read(lua_State* L) {
    ikcpcb* kcp = (ikcpcb*)(lua_touserdata(L, 1));
    if (kcp == NULL) { return luaL_error(L, "null kcp pointer"); }
    box* ud = (box*)kcp->user;
    if (ud->session != 0)
        return luaL_error(L, "already has a read request!");
    ud->session = (int32_t)luaL_checkinteger(L, 2);
    auto n = luaL_checkinteger(L, 3);
    if(n<=0)
        return luaL_error(L, "invalid read size!");
    ud->readn = n;
    return 0;
}

extern "C" {
    int LUAMOD_API luaopen_kcp_core(lua_State* L)
    {
        luaL_Reg l[] = {
            { "create", lua_ikcp_create},
            { "release", lua_ikcp_release },
            { "update", lua_ikcp_update },
            { "input", lua_ikcp_input },
            { "output", lua_ikcp_output },
            { "send", lua_ikcp_send},
            { "read", lua_ikcp_read},
            { "poll_read", lua_ikcp_poll_read},
            {NULL,NULL}
        };
        luaL_newlib(L, l);
        return 1;
    }
}
