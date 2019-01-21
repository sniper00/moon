
#include "lua.hpp"
#include <stdlib.h>
#include <string.h>
#include "quad_tree.hpp"

#define METANAME "laoi"

struct aoi_space_box
{
    using quad_tree_t = math::quad_tree<64>;
    quad_tree_t* q;
};

static int lrelease(lua_State *L)
{
    aoi_space_box* ab = (aoi_space_box*)lua_touserdata(L, 1);
    if (ab&&ab->q)
    {
        delete ab->q;
        ab->q = NULL;
    }
    return 0;
}

static int laoi_insert(lua_State *L)
{
    aoi_space_box* ab = (aoi_space_box*)lua_touserdata(L, 1);
    if (ab == NULL || ab->q == NULL)
        return luaL_error(L, "Invalid aoi_space pointer");
    int32_t id = (int32_t)luaL_checkinteger(L, 2);
    float x = (float)luaL_checknumber(L, 3);
    float y = (float)luaL_checknumber(L, 4);
    ab->q->insert(id, x, y);
    return 0;
}

static int laoi_update(lua_State *L)
{
    aoi_space_box* ab = (aoi_space_box*)lua_touserdata(L, 1);
    if (ab == NULL || ab->q == NULL)
        return luaL_error(L, "Invalid aoi_space pointer");
    int32_t id = (int32_t)luaL_checkinteger(L, 2);
    float x = (float)luaL_checknumber(L, 3);
    float y = (float)luaL_checknumber(L, 4);
    ab->q->update(id, x, y);
    return 0;
}

static std::vector<math::objectid_t> query_cache;

static int laoi_query(lua_State *L)
{
    aoi_space_box* ab = (aoi_space_box*)lua_touserdata(L, 1);
    if (ab == NULL || ab->q == NULL)
        return luaL_error(L, "Invalid aoi_space pointer");
    int32_t id = (int32_t)luaL_checkinteger(L, 2);
    float w = (float)luaL_checknumber(L, 3);
    float h = (float)luaL_checknumber(L, 4);
    int version = (int)luaL_checkinteger(L, 5);
    if (version >= 250)
    {
        version = 1;
    }
    version+=2;//
    luaL_checktype(L, 6, LUA_TTABLE);
    query_cache.clear();
    ab->q->query(id, w, h, query_cache);
    for (auto n : query_cache)
    {
        if (n == id)
        {
            continue;
        }
        int nv = version;
        lua_pushinteger(L, n);
        lua_pushvalue(L, -1);
        if (LUA_TNIL != lua_rawget(L, 6))// already in view
        {
            nv = version-1;
        }
        lua_pop(L, 1);
        lua_pushinteger(L, nv);
        lua_rawset(L, 6);
    }
    lua_pushinteger(L, version);
    return 1;
}

static int laoi_erase(lua_State *L)
{
    aoi_space_box* ab = (aoi_space_box*)lua_touserdata(L, 1);
    if (ab == NULL || ab->q == NULL)
        return luaL_error(L, "Invalid aoi_space pointer");
    int32_t id = (int32_t)luaL_checkinteger(L, 2);
    ab->q->erase(id);
    return 0;
}

static int laoi_create(lua_State *L)
{
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float w = (float)luaL_checknumber(L, 3);
    float h = (float)luaL_checknumber(L, 4);

    auto* q = new aoi_space_box::quad_tree_t(math::node_rect(x, y, w, h));
    aoi_space_box* ab = (aoi_space_box*)lua_newuserdata(L, sizeof(*ab));
    ab->q = q;
    if (luaL_newmetatable(L, METANAME))//mt
    {
        luaL_Reg l[] = {
            { "insert",laoi_insert },
            { "update",laoi_update },
            { "query",laoi_query },
            { "erase",laoi_erase },
            { NULL,NULL }
        };
        luaL_newlib(L, l); //{}
        lua_setfield(L, -2, "__index");//mt[__index] = {}
        lua_pushcfunction(L, lrelease);
        lua_setfield(L, -2, "__gc");//mt[__gc] = lrelease
    }
    lua_setmetatable(L, -2);// set userdata metatable
    lua_pushlightuserdata(L, ab);
    return 2;
}


extern "C"
{
    int LUAMOD_API luaopen_aoi(lua_State *L)
    {
        luaL_Reg l[] = {
            {"create",laoi_create},
            {"release",lrelease },
            {NULL,NULL}
        };
        luaL_checkversion(L);
        luaL_newlib(L, l);
        return 1;
    }
}
