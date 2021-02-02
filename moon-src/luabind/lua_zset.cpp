#include "common/zset.hpp"
#include "lua.hpp"

#define METANAME "lzet"

struct zset_proxy
{
    moon::zset* zset;
};

static int lupdate(lua_State* L)
{
    zset_proxy* proxy = (zset_proxy*)lua_touserdata(L, 1);
    if (nullptr == proxy || nullptr == proxy->zset)
    {
        return luaL_error(L, "invalid lua-zset pointer");
    }

    int64_t key = (int64_t)luaL_checkinteger(L, 2);
    int64_t score = (int64_t)luaL_checkinteger(L, 3);
    int64_t timestamp = (int64_t)luaL_checkinteger(L, 4);
    proxy->zset->update(key, score, timestamp);
    return 0;
}

static int lprepare(lua_State* L)
{
    zset_proxy* proxy = (zset_proxy*)lua_touserdata(L, 1);
    if (nullptr == proxy || nullptr == proxy->zset)
    {
        return luaL_error(L, "invalid lua-zset pointer");
    }
    proxy->zset->prepare();
    return 0;
}

static int lrank(lua_State* L)
{
    zset_proxy* proxy = (zset_proxy*)lua_touserdata(L, 1);
    if (nullptr == proxy || nullptr == proxy->zset)
    {
        return luaL_error(L, "invalid lua-zset pointer");
    }
    int64_t key = (int64_t)luaL_checkinteger(L, 2);
    lua_pushinteger(L, proxy->zset->rank(key));
    return 1;
}

static int lkey(lua_State* L)
{
    zset_proxy* proxy = (zset_proxy*)lua_touserdata(L, 1);
    if (nullptr == proxy || nullptr == proxy->zset)
    {
        return luaL_error(L, "invalid lua-zset pointer");
    }
    uint32_t rank = (uint32_t)luaL_checkinteger(L, 2);
    proxy->zset->prepare();
    auto iter = proxy->zset->start(rank);
    if (iter == proxy->zset->end())
    {
        return 0;
    }
    lua_pushinteger(L, (*iter)->key);
    return 1;
}

static int lscore(lua_State* L)
{
    zset_proxy* proxy = (zset_proxy*)lua_touserdata(L, 1);
    if (nullptr == proxy || nullptr == proxy->zset)
    {
        return luaL_error(L, "invalid lua-zset pointer");
    }
    int64_t key = (int64_t)luaL_checkinteger(L, 2);
    lua_pushinteger(L, proxy->zset->score(key));
    return 1;
}

static int lhas(lua_State* L)
{
    zset_proxy* proxy = (zset_proxy*)lua_touserdata(L, 1);
    if (nullptr == proxy || nullptr == proxy->zset)
    {
        return luaL_error(L, "invalid lua-zset pointer");
    }
    int64_t key = (int64_t)luaL_checkinteger(L, 2);
    lua_pushboolean(L, proxy->zset->has(key)?1:0);
    return 1;
}

static int lsize(lua_State* L)
{
    zset_proxy* proxy = (zset_proxy*)lua_touserdata(L, 1);
    if (nullptr == proxy || nullptr == proxy->zset)
    {
        return luaL_error(L, "invalid lua-zset pointer");
    }
    lua_pushinteger(L, proxy->zset->size());
    return 1;
}

static int lclear(lua_State* L)
{
    zset_proxy* proxy = (zset_proxy*)lua_touserdata(L, 1);
    if (nullptr == proxy || nullptr == proxy->zset)
    {
        return luaL_error(L, "invalid lua-zset pointer");
    }
    proxy->zset->clear();
    return 0;
}

static int lerase(lua_State* L)
{
    zset_proxy* proxy = (zset_proxy*)lua_touserdata(L, 1);
    if (nullptr == proxy || nullptr == proxy->zset)
    {
        return luaL_error(L, "invalid lua-zset pointer");
    }
    int64_t key = (int64_t)luaL_checkinteger(L, 2);
    lua_pushinteger(L, proxy->zset->erase(key));
    return 1;
}

static int lrange(lua_State* L)
{
    zset_proxy* proxy = (zset_proxy*)lua_touserdata(L, 1);
    if (nullptr == proxy || nullptr == proxy->zset)
    {
        return luaL_error(L, "invalid lua-zset pointer");
    }
    uint32_t start = (uint32_t)luaL_checkinteger(L, 2);
    uint32_t end = (uint32_t)luaL_checkinteger(L, 3);

    auto count = end - start + 1;
    if (end < start || count == 0)
    {
        return 0;
    }
    proxy->zset->prepare();
    auto iter = proxy->zset->start(start);
    if (iter == proxy->zset->end())
    {
        return 0;
    }
    lua_createtable(L, std::min(32U, count), 0);
    int idx = 1;
    for (; iter != proxy->zset->end() && count > 0; ++iter)
    {
        lua_pushinteger(L, (*iter)->key);
        --count;
        lua_rawseti(L, -2, idx++);
    }
    return 1;
};

static int lrelease(lua_State* L)
{
    zset_proxy* proxy = (zset_proxy*)lua_touserdata(L, 1);
    if (proxy && proxy->zset)
    {
        delete proxy->zset;
        proxy->zset = nullptr;
    }
    return 0;
}

static int lcreate(lua_State* L)
{
    size_t max_count = (size_t)luaL_checkinteger(L, 1);
    zset_proxy* proxy = (zset_proxy*)lua_newuserdatauv(L, sizeof(zset_proxy), 0);
    proxy->zset = new moon::zset(max_count);
    if (luaL_newmetatable(L, METANAME))//mt
    {
        luaL_Reg l[] = {
            { "update", lupdate },
            { "prepare",lprepare },
            { "has", lhas},
            { "rank", lrank},
            { "key", lkey},
            { "score", lscore},
            { "range", lrange},
            { "clear", lclear},
            { "size", lsize},
            { "erase", lerase},
            { NULL,NULL }
        };
        luaL_newlib(L, l); //{}
        lua_setfield(L, -2, "__index");//mt[__index] = {}
        lua_pushcfunction(L, lrelease);
        lua_setfield(L, -2, "__gc");//mt[__gc] = lrelease
    }
    lua_setmetatable(L, -2);// set userdata metatable
    return 1;
}

extern "C"
{
    int LUAMOD_API luaopen_zset(lua_State* L)
    {
        luaL_Reg l[] = {
            {"new",lcreate},
            {"release",lrelease },
            {NULL,NULL}
        };
        luaL_checkversion(L);
        luaL_newlib(L, l);
        return 1;
    }
}
