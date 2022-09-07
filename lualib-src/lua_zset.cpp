#include <algorithm>
#include "common/zset.hpp"
#include "lua.hpp"

#define METANAME "lzet"

#ifdef MOON_ENABLE_MIMALLOC
#include "mimalloc.h"
using zset_type = moon::zset<mi_stl_allocator>;
#else
using zset_type = moon::zset<std::allocator>;
#endif

static int lupdate(lua_State* L)
{
    zset_type* zset = (zset_type*)lua_touserdata(L, 1);
    if (nullptr == zset)
        return luaL_argerror(L, 1, "invalid lua-zset pointer");

    try
    {
        int64_t key = (int64_t)luaL_checkinteger(L, 2);
        int64_t score = (int64_t)luaL_checkinteger(L, 3);
        int64_t timestamp = (int64_t)luaL_checkinteger(L, 4);
        zset->update(key, score, timestamp);
        return 0;
    }
    catch (const std::exception& ex)
    {
        lua_pushstring(L, ex.what());
    }
    return lua_error(L);
}

static int lrank(lua_State* L)
{
    zset_type* zset = (zset_type*)lua_touserdata(L, 1);
    if (nullptr == zset)
        return luaL_argerror(L, 1, "invalid lua-zset pointer");

    int64_t key = (int64_t)luaL_checkinteger(L, 2);
    auto v = zset->rank(key);
    if(v>0)
    {
        lua_pushinteger(L, zset->rank(key));
        return 1;
    }
    return 0;
}

static int lkey_by_rank(lua_State* L)
{
    zset_type* zset = (zset_type*)lua_touserdata(L, 1);
    if (nullptr == zset)
        return luaL_argerror(L, 1, "invalid lua-zset pointer");

    uint32_t rank = (uint32_t)luaL_checkinteger(L, 2);

    if (zset->size() == 0 || zset->size() < rank)
        return 0;
    
    zset_type::const_iterator it = (rank == 1 ? zset->begin() : zset->find_by_rank(rank));

    if (it != zset->end()) {
        lua_pushinteger(L, it->key);
        return 1;
    }
    return 0;
}

static int lscore(lua_State* L)
{
    zset_type* zset = (zset_type*)lua_touserdata(L, 1);
    if (nullptr == zset)
        return luaL_argerror(L, 1, "invalid lua-zset pointer");

    int64_t key = (int64_t)luaL_checkinteger(L, 2);
    lua_pushinteger(L, zset->score(key));
    return 1;
}

static int lhas(lua_State* L)
{
    zset_type* zset = (zset_type*)lua_touserdata(L, 1);
    if (nullptr == zset)
        return luaL_argerror(L, 1, "invalid lua-zset pointer");

    int64_t key = (int64_t)luaL_checkinteger(L, 2);
    lua_pushboolean(L, zset->has(key)?1:0);
    return 1;
}

static int lsize(lua_State* L)
{
    zset_type* zset = (zset_type*)lua_touserdata(L, 1);
    if (nullptr == zset)
        return luaL_argerror(L, 1, "invalid lua-zset pointer");

    lua_pushinteger(L, zset->size());
    return 1;
}

static int lclear(lua_State* L)
{
    zset_type* zset = (zset_type*)lua_touserdata(L, 1);
    if (nullptr == zset)
        return luaL_argerror(L, 1, "invalid lua-zset pointer");
    zset->clear();
    return 0;
}

static int lerase(lua_State* L)
{
    zset_type* zset = (zset_type*)lua_touserdata(L, 1);
    if (nullptr == zset)
        return luaL_argerror(L, 1, "invalid lua-zset pointer");
    int64_t key = (int64_t)luaL_checkinteger(L, 2);
    lua_pushinteger(L, zset->erase(key));
    return 1;
}

static int lrange(lua_State* L)
{
    zset_type* zset = (zset_type*)lua_touserdata(L, 1);
    if (nullptr == zset)
        return luaL_argerror(L, 1, "invalid lua-zset pointer");
    int64_t start = luaL_checkinteger(L, 2)-1;
    int64_t end = luaL_checkinteger(L, 3)-1;
    bool reverse = lua_toboolean(L, 4) != 0;
    int64_t llen = (int64_t)zset->size();

    if (start < 0)
        start = llen + start;
    if (end < 0)
        end = llen + end;
    if (start < 0)
        start = 0;

    if (start > end || start >= llen)
        return 0;
    if (end >= llen)
        end = llen - 1;

    int64_t ranglen = (end - start) + 1;

    if (ranglen >= std::numeric_limits<int>::max())
        return luaL_error(L, "zset.range out off limit");

    zset_type::const_iterator it{nullptr};
    if (reverse) {
        it = zset->tail();
        if (start > 0)
            it = zset->find_by_rank(llen - start);
    }
    else
    {
        it = zset->begin();
        if (start > 0)
            it = zset->find_by_rank(start + 1);
    }

    lua_createtable(L, (int)ranglen, 0);
    int idx = 1;
    while (ranglen--) {
        lua_pushinteger(L, it->key);
        lua_rawseti(L, -2, idx++);
        reverse ? --it : ++it;
    }
    return 1;
};

static int lrelease(lua_State* L)
{
    zset_type* zset = (zset_type*)lua_touserdata(L, 1);
    if (nullptr == zset)
        return luaL_argerror(L, 1, "invalid lua-zset pointer");
    std::destroy_at(zset);
    return 0;
}

static int lcreate(lua_State* L)
{
    size_t max_count = (size_t)luaL_checkinteger(L, 1);
    bool reverse = lua_toboolean(L, 2) != 0;
    void* p = lua_newuserdatauv(L, sizeof(zset_type), 0);
    new (p) zset_type(max_count, reverse);
    if (luaL_newmetatable(L, METANAME))//mt
    {
        luaL_Reg l[] = {
            { "update", lupdate },
            { "has", lhas},
            { "rank", lrank},
            { "key_by_rank", lkey_by_rank},
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

extern "C" {
    int LUAMOD_API luaopen_zset(lua_State* L)
    {
        luaL_Reg l[] = {
            {"new",lcreate},
            {"release",lrelease },
            {NULL,NULL}
        };
        luaL_newlib(L, l);
        return 1;
    }
}

