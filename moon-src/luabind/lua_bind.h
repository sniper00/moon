#pragma once
#include <string>
#include "sol/sol.hpp"
#include "common/noncopyable.hpp"

class lua_service;

namespace moon
{
    class server;
    class log;
}

class lua_bind :public moon::noncopyable
{
public:
    explicit lua_bind(sol::table& lua);
    ~lua_bind();

    const lua_bind& bind_timer(lua_service* s) const;

    const lua_bind& bind_util() const;

    const lua_bind& bind_log(lua_service* service) const;


    const lua_bind& bind_service(lua_service* s) const;

    const lua_bind& bind_socket(lua_service* s)const;

    static void registerlib(lua_State *L, const char *name, lua_CFunction f);

    static void registerlib(lua_State *L, const char *name, const sol::table&);
private:
    sol::table& lua;
};

extern "C"
{
    int lua_json_decode(lua_State* L, const char*, size_t);
    int custom_package_loader(lua_State* L);
    void open_custom_libraries(lua_State* L);
}

