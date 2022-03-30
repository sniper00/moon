#include "lua_service.h"
#include "message.hpp"
#include "server.h"
#include "worker.h"
#include "common/hash.hpp"
#include "common/lua_utility.hpp"

#ifdef MOON_ENABLE_MIMALLOC
#    include "mimalloc.h"
#    define free mi_free
#    define realloc mi_realloc
#endif

using namespace moon;

constexpr size_t mb_memory = 1024 * 1024;

extern "C" void open_custom_libs(lua_State* L);

void* lua_service::lalloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    lua_service* l = reinterpret_cast<lua_service*>(ud);
    size_t mem = l->mem;

    l->mem += nsize;

    if (ptr)
        l->mem -= osize;

    if (l->mem > l->mem_limit)
    {
        if (ptr == nullptr || nsize > osize)
        {
            l->mem = mem;
            l->logger()->logstring(true, moon::LogLevel::Error,
                moon::format("%s Memory error current %.2f M, limit %.2f M", l->name().data(), (float)(l->mem) / mb_memory,
                    (float)l->mem_limit / mb_memory),
                l->id());
            return nullptr;
        }
    }
    else if (l->mem > l->mem_report)
    {
        l->mem_report *= 2;
        l->logger()->logstring(
            true, moon::LogLevel::Warn, moon::format("%s Memory warning %.2f M", l->name().data(), (float)l->mem / mb_memory), l->id());
    }

    if (nsize == 0)
    {
        free(ptr);
        return nullptr;
    }
    else
    {
        return realloc(ptr, nsize);
    }
}

lua_service* lua_service::get(lua_State* L)
{
    void* ud = nullptr;
    lua_getallocf(L, &ud);
    if (ud != nullptr)
        return reinterpret_cast<lua_service*>(ud);
    luaL_error(L, "lua_service is not init!");
    return nullptr;
}

lua_service::lua_service()
    : lua_(lua_newstate(lalloc, this))
{}

lua_service::~lua_service()
{
    logger()->logstring(true, moon::LogLevel::Info, moon::format("[WORKER %u] destroy service [%s] ", worker_->id(), name().data()), id());
}

bool lua_service::init(const moon::service_conf& conf)
{
    auto protect_init = [](lua_State* L) -> int {
        lua_service* S = lua_service::get(L);
        try
        {
            const moon::service_conf* conf = (const moon::service_conf*)lua_touserdata(L, 1);
            lua_settop(L, 0);
            luaL_openlibs(L);
            open_custom_libs(L);

            lua_pushcfunction(L, traceback);
            assert(lua_gettop(L) == 1);

            int r = luaL_loadfile(L, conf->source.data());
            if (r != LUA_OK)
                return luaL_error(L, "loadfile %s", lua_tostring(L, -1));

            int nargs = 0;
            {
                std::string initialize = S->server_->get_env("PATH");
                nargs = (conf->params.empty() ? 0 : 1);
                if (nargs > 0)
                    initialize.append(conf->params);
                r = luaL_dostring(L, initialize.data());
            }
            if (r != LUA_OK)
                return luaL_error(L, "initialize %s", lua_tostring(L, -1));

            r = lua_pcall(L, nargs, 0, 1);
            if (r != LUA_OK)
                return luaL_error(L, "run %s", lua_tostring(L, -1));

            lua_settop(L, 0);

            if (S->unique() && !S->server_->set_unique_service(S->name(), S->id()))
                return luaL_error(L, "duplicate unique service name '%s'.", S->name().data());

            S->ok_ = true;
        }
        catch (const std::exception& e)
        {
            CONSOLE_ERROR(S->logger(), "new_service exception:\n%s.", e.what());
        }
        return 0;
    };

    mem_limit = conf.memlimit;
    name_ = conf.name;

    lua_State* L = lua_.get();

    lua_gc(L, LUA_GCSTOP, 0);
    lua_gc(L, LUA_GCGEN, 0, 0);

    lua_pushcfunction(L, protect_init);
    lua_pushlightuserdata(L, (void*)&conf);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK)
    {
        CONSOLE_ERROR(logger(), "new_service lua_error:\n%s.", lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }

    if (!ok_)
        return false;

    lua_gc(L, LUA_GCRESTART, 0);

    logger()->logstring(true, moon::LogLevel::Info, moon::format("[WORKER %u] new service [%s]", worker_->id(), name().data()), id());

    return ok_;
}

void lua_service::dispatch(message* msg)
{
    if (!ok())
        return;
    lua_State* L = lua_.get();
    try
    {
        int trace = 1;
        int top = lua_gettop(L);
        if (top == 0)
        {
            lua_pushcfunction(L, traceback);
            lua_rawgetp(L, LUA_REGISTRYINDEX, this);
        }
        else
        {
            assert(top == 2);
        }
        lua_pushvalue(L, 2);

        lua_pushlightuserdata(L, msg);
        lua_pushinteger(L, msg->type());

        int r = lua_pcall(L, 2, 0, trace);
        if (r == LUA_OK)
        {
            return;
        }

        std::string error;

        switch (r)
        {
        case LUA_ERRRUN:
            error = moon::format("dispatch %s error:\n%s", name().data(), lua_tostring(L, -1));
            break;
        case LUA_ERRMEM:
            error = moon::format("dispatch %s memory error", name().data());
            break;
        case LUA_ERRERR:
            error = moon::format("dispatch %s error in error", name().data());
            break;
        };

        lua_pop(L, 1);

        if (msg->sessionid() >= 0)
        {
            logger()->logstring(true, moon::LogLevel::Error, error, id());
        }
        else
        {
            msg->set_sessionid(-msg->sessionid());
            server_->response(msg->sender(), "dispatch "sv, error, msg->sessionid(), PTYPE_ERROR);
        }
    }
    catch (const std::exception& e)
    {
        luaL_traceback(L, L, e.what(), 1);
        const char* trace = lua_tostring(L, -1);
        if (nullptr == trace)
        {
            trace = "";
        }
        CONSOLE_ERROR(logger(), "dispatch:\n%s", trace);
        lua_pop(L, 1);
    }
}
