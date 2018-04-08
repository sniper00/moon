#pragma once
#include <string>
#include "sol.hpp"
#include "common/noncopyable.hpp"
#include "common/timer.hpp"

class lua_service;

namespace moon
{
    class server;
    class log;
}

class lua_bind:public moon::noncopyable
{
public:
    explicit lua_bind(sol::table& lua);
    ~lua_bind();

    const lua_bind& bind_timer(moon::timer* t) const;

    const lua_bind& bind_util() const;

    const lua_bind& bind_path() const;

    const lua_bind& bind_log(moon::log* logger) const;

    const lua_bind& bind_message() const;

    const lua_bind& bind_service(lua_service* s) const;

    const lua_bind& bind_socket()const;

    const lua_bind& bind_http() const;
private:
    sol::table& lua;
};

const char* lua_traceback(lua_State* _state);

extern "C" 
{
    extern int luaopen_rapidjson(lua_State* L);
    int  luaopen_protobuf_c(lua_State *L);
}

