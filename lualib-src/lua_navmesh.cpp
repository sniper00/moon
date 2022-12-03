#include "lua.hpp"
#include "common/navmesh.hpp"
#include "common/lua_utility.hpp"

#define METANAME "lnavmesh"

using navmesh_type = moon::navmesh;

static int load_static(lua_State* L)
{
    auto meshfile = moon::lua_check<std::string>(L, 1);
    std::string err;
    if (moon::navmesh::load_static(meshfile, err))
    {
        lua_pushboolean(L, 1);
        return 1;
    }
    lua_pushboolean(L, 0);
    lua_pushlstring(L, err.data(), err.size());
    return 2;
}

static int load_dynamic(lua_State* L)
{
    navmesh_type* p = (navmesh_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_error(L, "Invalid navmesh pointer");

    auto meshfile = moon::lua_check<std::string>(L, 2);
    std::string err;
    if (p->load_dynamic(meshfile, err))
    {
        lua_pushboolean(L, 1);
        return 1;
    }
    lua_pushboolean(L, 0);
    lua_pushlstring(L, err.data(), err.size());
    return 2;
}

static int find_straight_path(lua_State* L)
{
    navmesh_type* p = (navmesh_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_error(L, "Invalid navmesh pointer");
    auto sx = moon::lua_check<float>(L, 2);
    auto sy = moon::lua_check<float>(L, 3);
    auto sz = moon::lua_check<float>(L, 4);
    auto ex = moon::lua_check<float>(L, 5);
    auto ey = moon::lua_check<float>(L, 6);
    auto ez = moon::lua_check<float>(L, 7);
    std::vector<float> paths;
    if (p->find_straight_path(sx, sy, sz, ex, ey, ez, paths))
    {
        lua_createtable(L, (int)paths.size(), 0);
        for (size_t i = 0; i < paths.size(); ++i)
        {
            lua_pushnumber(L, paths[i]);
            lua_rawseti(L, -2, i + 1);
        }
        return 1;
    }
    lua_pushboolean(L, 0);
    lua_pushlstring(L, p->get_status().data(), p->get_status().size());
    return 2;
}

static int valid(lua_State* L)
{
    navmesh_type* p = (navmesh_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_error(L, "Invalid navmesh pointer");
    auto x = moon::lua_check<float>(L, 2);
    auto y = moon::lua_check<float>(L, 3);
    auto z = moon::lua_check<float>(L, 4);
    bool res = p->valid(x, y, z);
    lua_pushboolean(L, res);
    return 1;
}

static int random_position(lua_State* L)
{
    navmesh_type* p = (navmesh_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_error(L, "Invalid navmesh pointer");
    float pos[3];
    if (p->random_position(pos))
    {
        lua_pushnumber(L, pos[0]);
        lua_pushnumber(L, pos[1]);
        lua_pushnumber(L, pos[2]);
        return 3;
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int random_position_around_circle(lua_State* L)
{
    navmesh_type* p = (navmesh_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_error(L, "Invalid navmesh pointer");

    auto x = moon::lua_check<float>(L, 2);
    auto y = moon::lua_check<float>(L, 3);
    auto z = moon::lua_check<float>(L, 4);
    auto r = moon::lua_check<float>(L, 5);

    float pos[3];
    if (p->random_position_around_circle(x, y, z, r, pos))
    {
        lua_pushnumber(L, pos[0]);
        lua_pushnumber(L, pos[1]);
        lua_pushnumber(L, pos[2]);
        return 3;
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int recast(lua_State* L)
{
    navmesh_type* p = (navmesh_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_error(L, "Invalid navmesh pointer");

    auto sx = moon::lua_check<float>(L, 2);
    auto sy = moon::lua_check<float>(L, 3);
    auto sz = moon::lua_check<float>(L, 4);
    auto ex = moon::lua_check<float>(L, 5);
    auto ey = moon::lua_check<float>(L, 6);
    auto ez = moon::lua_check<float>(L, 7);

    float hitPos[3];
    bool ok = p->recast(sx, sy, sz, ex, ey, ez, hitPos);
    lua_pushboolean(L, ok);
    lua_pushnumber(L, hitPos[0]);
    lua_pushnumber(L, hitPos[1]);
    lua_pushnumber(L, hitPos[2]);
    return 4;
}

static int add_capsule_obstacle(lua_State* L)
{
    navmesh_type* p = (navmesh_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_error(L, "Invalid navmesh pointer");

    auto x = moon::lua_check<float>(L, 2);
    auto y = moon::lua_check<float>(L, 3);
    auto z = moon::lua_check<float>(L, 4);
    auto r = moon::lua_check<float>(L, 5);
    auto h = moon::lua_check<float>(L, 6);
    auto obstacleId = p->add_capsule_obstacle(x, y, z, r, h);
    if (obstacleId > 0)
    {
        lua_pushinteger(L, obstacleId);
        return 1;
    }
    return 0;
}

static int remove_obstacle(lua_State* L)
{
    navmesh_type* p = (navmesh_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_error(L, "Invalid navmesh pointer");

    auto obstacleId = moon::lua_check<dtObstacleRef>(L, 2);
    auto res = p->remove_obstacle(obstacleId);
    lua_pushboolean(L, res);
    return 1;
}

static int clear_all_obstacle(lua_State* L)
{
    navmesh_type* p = (navmesh_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_error(L, "Invalid navmesh pointer");
    p->clear_all_obstacle();
    return 0;
}

static int update(lua_State* L)
{
    navmesh_type* p = (navmesh_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_error(L, "Invalid navmesh pointer");
    auto dt = moon::lua_check<float>(L, 2);
    p->update(dt);
    return 0;
}

static int lrelease(lua_State* L)
{
    navmesh_type* p = (navmesh_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_error(L, "Invalid navmesh pointer");
    std::destroy_at(p);
    return 0;
}

static int lcreate(lua_State* L)
{
    std::string meshfile = luaL_optstring(L, 1, "");
    int mask =  (int)luaL_optinteger(L, 2, 0);

    navmesh_type* p = (navmesh_type*)lua_newuserdatauv(L, sizeof(navmesh_type), 0);
    new (p) navmesh_type(meshfile, mask);

    if (luaL_newmetatable(L, METANAME))//mt
    {
        luaL_Reg l[] = {
            { "load_dynamic",load_dynamic },
            { "find_straight_path",find_straight_path },
            { "valid", valid},
            { "random_position",random_position },
            { "random_position_around_circle",random_position_around_circle },
            { "recast",recast },
            { "add_capsule_obstacle",add_capsule_obstacle },
            { "remove_obstacle", remove_obstacle},
            { "clear_all_obstacle", clear_all_obstacle},
            { "update", update},
            { NULL,NULL }
        };
        luaL_newlib(L, l); //{}
        lua_setfield(L, -2, "__index");//mt[__index] = {}
        lua_pushcfunction(L, lrelease);
        lua_setfield(L, -2, "__gc");//mt[__gc] = lrelease
    }
    lua_setmetatable(L, -2);// set userdata metatable
    lua_pushlightuserdata(L, p);
    return 2;
}

extern "C" {
    int LUAMOD_API luaopen_navmesh(lua_State* L)
    {
        luaL_Reg l[] = {
            {"new",lcreate},
            {"load_static",load_static},
            {NULL,NULL}
        };
        luaL_newlib(L, l);
        return 1;
    }
}
