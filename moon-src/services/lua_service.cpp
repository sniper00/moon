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

constexpr ssize_t mb_memory = 1024 * 1024;

extern "C" void open_custom_libs(lua_State * L);

void* lua_service::lalloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    lua_service* l = reinterpret_cast<lua_service*>(ud);

    if (nsize == 0)
    {
        if (ptr) {
            free(ptr);
            l->mem -= osize;
        }
        return nullptr;
    }

    if (nsize > static_cast<size_t>(std::numeric_limits<ssize_t>::max()))
    {
        return nullptr;
    }

    auto mem_diff = static_cast<ssize_t>(nsize);
    if(nullptr != ptr)
    {
        mem_diff -= static_cast<ssize_t>(osize);
    }

    auto new_used_memory = l->mem + mem_diff;

    if (new_used_memory > l->mem_limit)
    {
        if (ptr == nullptr || nsize > osize)
        {
            log::instance().logstring(true, moon::LogLevel::Error,
                moon::format("%s Memory error current %.2f M, limit %.2f M", l->name().data(), (float)(l->mem) / mb_memory,
                    (float)l->mem_limit / mb_memory),
                l->id());
            return nullptr;
        }
    }
    
    l->mem += mem_diff;
    
    if (new_used_memory > l->mem_report)
    {
        l->mem_report *= 2;
        log::instance().logstring(
            true, moon::LogLevel::Warn, moon::format("%s Memory warning %.2f M", l->name().data(), (float)l->mem / mb_memory), l->id());
    }

    return realloc(ptr, nsize);
}

lua_service* lua_service::get(lua_State* L)
{
    static_assert((LUA_EXTRASPACE == sizeof(lua_service*)) && (LUA_EXTRASPACE == sizeof(intptr_t)));
    intptr_t v = 0;
    memcpy(&v, lua_getextraspace(L), LUA_EXTRASPACE);
    return reinterpret_cast<lua_service*>(v);
}

lua_service::lua_service()
    : lua_(lua_newstate(lalloc, this))
{
    static_assert((LUA_EXTRASPACE == sizeof(this)) && (LUA_EXTRASPACE == sizeof(intptr_t)));
    intptr_t p = (intptr_t)this;
    memcpy(lua_getextraspace(lua_.get()), &p, LUA_EXTRASPACE);
}

lua_service::~lua_service()
{
    log::instance().logstring(true, moon::LogLevel::Info, moon::format("[WORKER %u] destroy service [%s] ", worker_->id(), name().data()), id());
}

static int protect_init(lua_State* L)
{
    const moon::service_conf* conf = (const moon::service_conf*)lua_touserdata(L, 1);
    
    if (nullptr == conf) {
        luaL_error(L, "Invalid service conf");
        return 0;
    }

    luaL_openlibs(L);
    open_custom_libs(L);

    if ((luaL_loadfile(L, conf->source.data())) != LUA_OK)
        return 1;

    if ((luaL_dostring(L, conf->params.data())) != LUA_OK)
        return 1;

    lua_call(L, 1, 0);
    return 0;
}

bool lua_service::init(const moon::service_conf& conf)
{
    mem_limit = conf.memlimit;
    name_ = conf.name;

    log::instance().logstring(true, moon::LogLevel::Info, moon::format("[WORKER %u] new service [%s]", worker_->id(), name().data()), id());

    lua_State* L = lua_.get();

    lua_gc(L, LUA_GCSTOP, 0);
    lua_gc(L, LUA_GCGEN, 0, 0);


    lua_pushcfunction(L, traceback);
    int trace_fn = lua_gettop(L);

    lua_pushcfunction(L, protect_init);
    lua_pushlightuserdata(L, (void*)&conf);

    if (lua_pcall(L, 1, LUA_MULTRET, trace_fn) != LUA_OK || lua_gettop(L) > 1)
    {
        CONSOLE_ERROR("new_service lua_error:\n%s.", lua_tostring(L, -1));
        return false;
    }

    lua_pop(L, 1);

    if (unique_ && !server_->set_unique_service(name_, id_))
    {
        CONSOLE_ERROR("duplicate unique service name '%s'.", name_.data());
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

int64_t lua_service::next_sequence()
{
    auto v = ++current_sequence_;
    if(v==std::numeric_limits<int64_t>::max()){//Should never happened!
        CONSOLE_ERROR("sequence rewinded!!!");
        server_->stop(-1);
    }
    return v;
}

void lua_service::dispatch(message* m)
{
    if (!ok())
        return;

    //require 'moon' first
    assert(cb_ctx != nullptr);

    lua_State* L = cb_ctx->L;
    try
    {
        int trace = 1;
        lua_pushvalue(L, 2);

        lua_pushinteger(L, m->type());
        lua_pushinteger(L, m->sender());
        lua_pushinteger(L, m->sessionid());
        lua_pushlightuserdata(L, (void*)m->data());
        lua_pushinteger(L, m->size());
        lua_pushlightuserdata(L, m);
      
        int r = lua_pcall(L, 6, 0, trace);
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
        }

        lua_pop(L, 1);

        if (m->sessionid() >= 0)
        {
            log::instance().logstring(true, moon::LogLevel::Error, error, id());
        }
        else
        {
            m->set_sessionid(-m->sessionid());
            server_->response(m->sender(), error, m->sessionid(), PTYPE_ERROR);
        }
    }
    catch (const std::exception& e)
    {
        luaL_traceback(L, L, e.what(), 1);
        const char* trace = lua_tostring(L, -1);
        CONSOLE_ERROR("dispatch:\n%s", (trace!=nullptr)?trace:"");
        lua_pop(L, 1);
    }
}
