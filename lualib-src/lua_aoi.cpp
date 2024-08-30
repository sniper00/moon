
#include "common/aoi.hpp"
#include "lua.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#define METANAME "laoi"

struct aoi_object {
    using handle_type = int64_t;
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    int32_t layer;
    int32_t mode;
    handle_type handle;

    aoi_object(
        int32_t x_,
        int32_t y_,
        int32_t w_,
        int32_t h_,
        int32_t layer_,
        int32_t mode_,
        handle_type handle_
    ):
        x(x_),
        y(y_),
        w(w_),
        h(h_),
        layer(layer_),
        mode(mode_),
        handle(handle_) {}

    //is this object contains by rc
    template<typename Rect>
    bool inside(const Rect& rc) {
        return rc.contains(x, y);
    }

    bool check() {
        return true;
    }
};

using aoi_type = aoi<aoi_object>;

static int lrelease(lua_State* L) {
    aoi_type* p = (aoi_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_argerror(L, 1, "invalid lua-aoi pointer");
    std::destroy_at(p);
    return 0;
}

static int laoi_insert(lua_State* L) {
    aoi_type* p = (aoi_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_argerror(L, 1, "invalid lua-aoi pointer");
    auto id = (aoi_object::handle_type)luaL_checkinteger(L, 2);
    int32_t x = (int32_t)luaL_checknumber(L, 3);
    int32_t y = (int32_t)luaL_checknumber(L, 4);
    int32_t view_w = (int32_t)luaL_checkinteger(L, 5);
    int32_t view_h = (int32_t)luaL_checkinteger(L, 6);
    int32_t layer = (int32_t)luaL_checkinteger(L, 7);
    int32_t mode = (int32_t)luaL_checkinteger(L, 8);
    p->clear_event();
    bool res = p->insert(id, x, y, view_w, view_h, layer, mode);
    lua_pushboolean(L, res);
    return 1;
}

static int laoi_update(lua_State* L) {
    aoi_type* p = (aoi_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_argerror(L, 1, "invalid lua-aoi pointer");
    auto id = (aoi_object::handle_type)luaL_checkinteger(L, 2);
    int32_t x = (int32_t)luaL_checknumber(L, 3);
    int32_t y = (int32_t)luaL_checknumber(L, 4);
    int32_t view_w = (int32_t)luaL_checkinteger(L, 5);
    int32_t view_h = (int32_t)luaL_checkinteger(L, 6);
    int32_t layer = (int32_t)luaL_checkinteger(L, 7);
    p->clear_event();
    auto res = p->update(id, x, y, view_w, view_h, layer);
    lua_pushboolean(L, res);
    return 1;
}

static int laoi_query(lua_State* L) {
    aoi_type* p = (aoi_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_argerror(L, 1, "invalid lua-aoi pointer");
    int32_t x = (int32_t)luaL_checknumber(L, 2);
    int32_t y = (int32_t)luaL_checknumber(L, 3);
    int32_t view_w = (int32_t)luaL_checkinteger(L, 4);
    int32_t view_h = (int32_t)luaL_checkinteger(L, 5);
    luaL_checktype(L, 6, LUA_TTABLE);

    std::vector<aoi_object::handle_type> vec;
    p->query(x, y, view_w, view_h, vec);
    if (vec.empty()) {
        return 0;
    }

    int idx = 1;
    for (const auto& id: vec) {
        lua_pushinteger(L, id);
        lua_rawseti(L, 6, idx);
        ++idx;
    }

    lua_pushinteger(L, static_cast<int64_t>(vec.size()));
    return 1;
}

static int laoi_erase(lua_State* L) {
    aoi_type* p = (aoi_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_argerror(L, 1, "invalid lua-aoi pointer");
    auto id = (aoi_object::handle_type)luaL_checkinteger(L, 2);
    p->clear_event();
    p->erase(id);
    return 0;
}

static int laoi_hasobject(lua_State* L) {
    aoi_type* p = (aoi_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_argerror(L, 1, "invalid lua-aoi pointer");
    auto id = (aoi_object::handle_type)luaL_checkinteger(L, 2);
    int res = p->has_object(id) ? 1 : 0;
    lua_pushboolean(L, res);
    return 1;
}

static int laoi_fire_event(lua_State* L) {
    aoi_type* p = (aoi_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_argerror(L, 1, "invalid lua-aoi pointer");
    auto id = (aoi_object::handle_type)luaL_checkinteger(L, 2);
    int32_t nevent = (int32_t)luaL_checkinteger(L, 3);
    p->clear_event();
    p->fire_event(id, nevent);
    return 0;
}

static int laoi_update_event(lua_State* L) {
    aoi_type* p = (aoi_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_argerror(L, 1, "invalid lua-aoi pointer");

    luaL_checktype(L, 2, LUA_TTABLE);

    const auto& events = p->get_event();
    if (events.empty()) {
        lua_pushinteger(L, 0);
        return 1;
    }

    int idx = 1;
    for (const auto& evt: events) {
        lua_pushinteger(L, evt.watcher);
        lua_rawseti(L, 2, idx++);
        lua_pushinteger(L, evt.marker);
        lua_rawseti(L, 2, idx++);
        lua_pushinteger(L, evt.eventid);
        lua_rawseti(L, 2, idx++);
    }

    lua_pushinteger(L, static_cast<int64_t>(events.size() * 3));
    return 1;
}

static int laoi_enable_debug(lua_State* L) {
    aoi_type* p = (aoi_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_argerror(L, 1, "invalid lua-aoi pointer");
    bool v = (bool)lua_toboolean(L, 2);
    p->enable_debug(v);
    return 0;
}

static int laoi_enable_leave_event(lua_State* L) {
    aoi_type* p = (aoi_type*)lua_touserdata(L, 1);
    if (nullptr == p)
        return luaL_argerror(L, 1, "invalid lua-aoi pointer");
    bool v = (bool)lua_toboolean(L, 2);
    p->enbale_leave_event(v);
    return 0;
}

static int laoi_create(lua_State* L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int len_of_area = (int)luaL_checkinteger(L, 3);
    int len_of_node = (int)luaL_checkinteger(L, 4);
    if (len_of_area % len_of_node != 0) {
        return luaL_error(L, "Need length_of_area %% length_of_node == 0.");
    }

    aoi_type* p = (aoi_type*)lua_newuserdatauv(L, sizeof(aoi_type), 0);
    new (p) aoi_type(x, y, len_of_area, len_of_node);

    if (luaL_newmetatable(L, METANAME)) //mt
    {
        luaL_Reg l[] = { { "insert", laoi_insert },
                         { "update", laoi_update },
                         { "query", laoi_query },
                         { "fire_event", laoi_fire_event },
                         { "erase", laoi_erase },
                         { "has", laoi_hasobject },
                         { "update_event", laoi_update_event },
                         { "enable_debug", laoi_enable_debug },
                         { "enable_leave_event", laoi_enable_leave_event },
                         { NULL, NULL } };
        luaL_newlib(L, l); //{}
        lua_setfield(L, -2, "__index"); //mt[__index] = {}
        lua_pushcfunction(L, lrelease);
        lua_setfield(L, -2, "__gc"); //mt[__gc] = lrelease
    }
    lua_setmetatable(L, -2); // set userdata metatable
    return 1;
}

extern "C" {
int LUAMOD_API luaopen_aoi(lua_State* L) {
    luaL_Reg l[] = { { "new", laoi_create }, { NULL, NULL } };
    luaL_newlib(L, l);
    return 1;
}
}
