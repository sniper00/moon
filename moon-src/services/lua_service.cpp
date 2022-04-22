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

extern "C" void open_custom_libs(lua_State * L);

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

static int protect_init(lua_State* L)
{
    const char* source = (const char*)lua_touserdata(L, 1);
    const char* initialize_string = (const char*)lua_touserdata(L, 2);

    luaL_openlibs(L);
    open_custom_libs(L);

    if ((luaL_loadfile(L, source)) != LUA_OK)
    {
        lua_pushfstring(L, "loadfile %s", lua_tostring(L, -1));
        return 1;
    }

    int nparam = lua_gettop(L);
    if ((luaL_dostring(L, initialize_string)) != LUA_OK)
    {
        lua_pushfstring(L, "initialize %s", lua_tostring(L, -1));
        return 1;
    }

    nparam = lua_gettop(L) - nparam;

    lua_call(L, nparam, 0);
    return 0;
};

bool lua_service::init(const moon::service_conf& conf)
{
    mem_limit = conf.memlimit;
    name_ = conf.name;

    logger()->logstring(true, moon::LogLevel::Info, moon::format("[WORKER %u] new service [%s]", worker_->id(), name().data()), id());

    lua_State* L = lua_.get();

    lua_gc(L, LUA_GCSTOP, 0);
    lua_gc(L, LUA_GCGEN, 0, 0);

    std::string initialize_string;
    auto lua_path = server_->get_env("PATH");
    if (lua_path)
        initialize_string.append(*lua_path);
    initialize_string.append(conf.params);

    lua_pushcfunction(L, traceback);
    int trace_fn = lua_gettop(L);

    lua_pushcfunction(L, protect_init);
    lua_pushlightuserdata(L, (void*)conf.source.data());
    lua_pushlightuserdata(L, initialize_string.data());

    if (lua_pcall(L, 2, LUA_MULTRET, trace_fn) != LUA_OK || lua_gettop(L) > 1)
    {
        CONSOLE_ERROR(logger(), "new_service lua_error:\n%s.", lua_tostring(L, -1));
        return false;
    }

	lua_pop(L, 1);

    if (unique_ && !server_->set_unique_service(name_, id_))
    {
        CONSOLE_ERROR(logger(), "duplicate unique service name '%s'.", name_.data());
        return false;
    }

    lua_gc(L, LUA_GCRESTART, 0);

    assert(lua_gettop(L) == 0);

    ok_ = true;

    return ok_;
}

//https://blog.codingnow.com/2022/04/lua_binding_callback.html#more
int lua_service::set_callback(lua_State* L) {
    lua_service* S = lua_service::get(L);
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_settop(L, 1);
    callback_context* cb_ctx = (callback_context*)lua_newuserdata(L, sizeof(callback_context)); // L [-0, +1]
    cb_ctx->L = lua_newthread(L);                                                               // L [-0, +1]
    lua_pushcfunction(cb_ctx->L, traceback);                                                    // cb_ctx->L [-0, +1]
    lua_setuservalue(L, -2);                                // L [-1, +0]  associate cb_ctx->L to the userdata
    lua_setfield(L, LUA_REGISTRYINDEX, "callback_context"); // L [-1, +0] ref userdata(cb_ctx), avoid free by GC
    lua_xmove(L, cb_ctx->L, 1); // L [-1, +0], cb_ctx->L [-0, +1] move callback-function to cb_ctx->L, cb_ctx->L's top=2
    S->cb_ctx = cb_ctx;
    return 0;
}

void lua_service::dispatch(message* msg)
{
    if (!ok())
        return;
    if (cb_ctx == nullptr) {
        logger()->logstring(true, moon::LogLevel::Error, "callback not init", id());
        return;
    }
    lua_State* L = cb_ctx->L;
    try
    {
        int trace = 1;
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
