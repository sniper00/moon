
#include "lua.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include "aoi.hpp"

#define METANAME "laoi"

struct aoi_object
{
    float x;
    float y;
    int32_t handle;
};

struct aoi_space_box
{
    using aoi_type = aoi<aoi_object>;
    aoi_type* space;
    std::vector<aoi_type::object_handle_type> query;
};

static int lrelease(lua_State *L)
{
    aoi_space_box* ab = (aoi_space_box*)lua_touserdata(L, 1);
    if (ab&&ab->space)
    {
        delete ab->space;
        ab->space = NULL;
    }
    return 0;
}

static int laoi_insert(lua_State *L)
{
    aoi_space_box* ab = (aoi_space_box*)lua_touserdata(L, 1);
    if (ab == NULL || ab->space == NULL)
        return luaL_error(L, "Invalid aoi_space pointer");
    int32_t id = (int32_t)luaL_checkinteger(L, 2);
    float x = (float)luaL_checknumber(L, 3);
    float y = (float)luaL_checknumber(L, 4);
    ab->space->insert(id, x, y);
    return 0;
}

static int laoi_update(lua_State *L)
{
    aoi_space_box* ab = (aoi_space_box*)lua_touserdata(L, 1);
    if (ab == NULL || ab->space == NULL)
        return luaL_error(L, "Invalid aoi_space pointer");
    int32_t id = (int32_t)luaL_checkinteger(L, 2);
    float x = (float)luaL_checknumber(L, 3);
    float y = (float)luaL_checknumber(L, 4);
    ab->space->update(id, x, y);
    return 0;
}

static int laoi_query(lua_State *L)
{
    aoi_space_box* ab = (aoi_space_box*)lua_touserdata(L, 1);
    if (ab == NULL || ab->space == NULL)
        return luaL_error(L, "Invalid aoi_space pointer");
    int32_t id = (int32_t)luaL_checkinteger(L, 2);
    float w = (float)luaL_checknumber(L, 3);
    float h = (float)luaL_checknumber(L, 4);
    int version = (int)luaL_checkinteger(L, 5);
    if (version >= 250)
    {
        version = 1;
    }
    version += 2;//
    luaL_checktype(L, 6, LUA_TTABLE);

    ab->space->query(id, w, h, [L, version, id](int32_t n) {
        if (n == id)
        {
            return;
        }
        int nv = version;
        lua_pushinteger(L, n);
        lua_pushvalue(L, -1);
        if (LUA_TNIL != lua_rawget(L, 6))// already in view
        {
            nv = version - 1;
        }
        lua_pop(L, 1);
        lua_pushinteger(L, nv);
        lua_rawset(L, 6);
    });
    lua_pushinteger(L, version);
    return 1;
}

static int laoi_erase(lua_State *L)
{
    aoi_space_box* ab = (aoi_space_box*)lua_touserdata(L, 1);
    if (ab == NULL || ab->space == NULL)
        return luaL_error(L, "Invalid aoi_space pointer");
    int32_t id = (int32_t)luaL_checkinteger(L, 2);
    ab->space->erase(id);
    return 0;
}

static int laoi_create(lua_State *L)
{
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int loa = (int)luaL_checkinteger(L, 3);
    int lon = (int)luaL_checkinteger(L, 4);
    if (loa%lon != 0)
    {
        return luaL_error(L, "Need length_of_area %% length_of_node == 0.");
    }

    auto* q = new aoi_space_box::aoi_type(x, y, loa, lon);
    aoi_space_box* ab = (aoi_space_box*)lua_newuserdata(L, sizeof(*ab));
    ab->space = q;
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
