#include "lua_service.h"
#include "message.hpp"
#include "server.h"
#include "common/string.hpp"
#include "rapidjson/document.h"
#include "rapidjson/rapidjson_helper.hpp"
#include "luabind/lua_serialize.hpp"
#include "service_config.hpp"

using namespace moon;

void * lua_service::lalloc(void * ud, void *ptr, size_t osize, size_t nsize) {
    lua_service *l = reinterpret_cast<lua_service*>(ud);
    size_t mem = l->mem;

    l->mem += nsize;

    if (ptr)
        l->mem -= osize;

    if (l->mem_limit != 0 && l->mem > l->mem_limit)
    {
        if (ptr == NULL || nsize > osize) {
            CONSOLE_ERROR(l->logger(),"%s Memory error current %.2f M, limit %.2f M", l->name().data(), (float)(l->mem) / (1024 * 1024), (float)l->mem_limit / (1024 * 1024));
            l->mem = mem;
            return NULL;
        }
    }

    if (l->mem > l->mem_report) {
        l->mem_report *= 2;
        CONSOLE_WARN(l->logger(),"%s Memory warning %.2f M",l->name().data(), (float)l->mem / (1024 * 1024));
    }

    if (nsize == 0)
    {
        free(ptr);
        return NULL;
    }
    else
    {
        return realloc(ptr, nsize);
    }
}

lua_service::lua_service()
    :lua_(sol::detail::default_at_panic,lalloc, this)
    ,error_(true)
{
}

lua_service::~lua_service()
{
}

moon::tcp * lua_service::add_component_tcp(const std::string & name)
{
    auto p = add_component<moon::tcp>(name);
    return ((p != nullptr) ? p.get() : nullptr);
}

moon::tcp * lua_service::get_component_tcp(const std::string & name)
{
    auto p = get_component<moon::tcp>(name);
    return ((p != nullptr) ? p.get() : nullptr);
}

void lua_service::set_init(sol_function_t f)
{
    init_ = f;
}

void lua_service::set_start(sol_function_t f)
{
    start_ = f;
}

void lua_service::set_dispatch(sol_function_t f)
{
    dispatch_ = f;
}

void lua_service::set_exit(sol_function_t f)
{
    exit_ = f;
}

void lua_service::set_destroy(sol_function_t f)
{
    destroy_ = f;
}

bool lua_service::init(const std::string& config)
{
    service_config<lua_service> scfg;
    if (!scfg.parse(this, config))
    {
        CONSOLE_ERROR(logger(), "Lua service parse config failed");
        return false;
    }

    auto luafile = scfg.get_value<std::string>("file");
    if (luafile.empty())
    {
        CONSOLE_ERROR(logger(),"Lua service init failed,does not provide luafile config.");
        return false;
    }

    mem_limit = static_cast<size_t>(scfg.get_value<int64_t>("memlimit"));

    {
        try
        {
            lua_.open_libraries();
            sol::table module = lua_.create_table();
            lua_bind lua_bind(module);
            lua_bind.bind_service(this)
                .bind_log(logger())
                .bind_util()
                .bind_path()
                .bind_timer(&timer_)
                .bind_message()
                .bind_socket()
                .bind_http();

            lua_.require("seri", lua_serialize::open);
            lua_.require("codecache", luaopen_cache);
            lua_.require("protobuf.c", luaopen_protobuf_c);
            lua_.require("json", luaopen_rapidjson);

            lua_["package"]["loaded"]["moon_core"] = module;

#if TARGET_PLATFORM == PLATFORM_WINDOWS
            lua_.script("package.cpath = './clib/?.dll;'");
#else
            lua_.script("package.cpath = './clib/?.so;'");
#endif
            lua_.script("package.path = './?.lua;./lualib/?.lua;'");
            lua_.script_file(luafile);

            if(init_.valid())
            {
                auto result = init_(config);
                error_ = result.valid() ? (!(bool)result): true;
            }
            else
            {
                error_ = false;
            }
            return  !error_;
        }
        catch (std::exception& e)
        {
            error(moon::format("lua_service::init\nfile:%s.\n%s\n", luafile.data(), e.what()));
        }
    }
    return false;
}

void lua_service::start()
{
    service::start();

    if (error_) return;
    try
    {    
        if (start_.valid())
        {
            start_();
        }
    }
    catch (std::exception& e)
    {
        error(moon::format("lua_service::start :\n%s\n", e.what()));
    }
}

void lua_service::dispatch(message* msg)
{
    if (error_) return;

    try
    {
        auto type = msg->type();
        //network message will directly handle,check it's messsage type.
        MOON_DCHECK(type != PTYPE_UNKNOWN, "recevice unknown type message");
        dispatch_(msg, type);
    }
    catch (std::exception& e)
    {
        error(moon::format("lua_service::dispatch:\n%s\n", e.what()));
    }
}

void lua_service::update()
{
    service::update();

    if (error_) return;
    try
    {
        timer_.update();
    }
    catch (std::exception& e)
    {
        error(moon::format("lua_service::update:\n%s\n", e.what()));
    }
}

void lua_service::exit()
{
    if (!error_)
    {
        try
        {
            if (exit_.valid())
            {
                exit_();
                return;
            }
        }
        catch (std::exception& e)
        {
            error(moon::format("lua_service::exit :%s\n", e.what()));
        }
    }
    service::exit();
}

void lua_service::destroy()
{
    if (!error_)
    {
        try
        {
            if (destroy_.valid())
            {
                destroy_();
            }
        }
        catch (std::exception& e)
        {
            error(moon::format("lua_service::destroy :%s\n", e.what()));
        }
    }
    service::destroy();
}

void lua_service::error(const std::string & msg)
{
    error_ = true;
    std::string backtrace = lua_traceback(lua_.lua_state());
    CONSOLE_ERROR(logger(),"%s %s", name().data(), (msg+backtrace).data());
    removeself(true);
    if (unique())
    {
        CONSOLE_ERROR(logger(), "unique server %s crashed, server will stop.", name().data());
        get_server()->stop();
    }
}

size_t lua_service::memory_use()
{
    return  mem;
}



