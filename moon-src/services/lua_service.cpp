#include "lua_service.h"
#include "message.hpp"
#include "server.h"
#include "worker.h"
#include "common/hash.hpp"
#include "rapidjson/document.h"
#include "service_config.hpp"
#include "server_config.hpp"


#ifdef MOON_ENABLE_MIMALLOC
#include "mimalloc.h"
#define free mi_free 
#define realloc mi_realloc 
#endif

using namespace moon;

constexpr size_t mb_memory = 1024 * 1024;

void *lua_service::lalloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    lua_service *l = reinterpret_cast<lua_service *>(ud);
    size_t mem = l->mem;

    l->mem += nsize;

    if (ptr)
        l->mem -= osize;

    if (l->mem_limit != 0 && l->mem > l->mem_limit)
    {
        if (ptr == nullptr || nsize > osize)
        {
            l->mem = mem;
            l->logger()->logstring(true, moon::LogLevel::Error,
                                   moon::format("%s Memory error current %.2f M, limit %.2f M", l->name().data(), (float)(l->mem) / mb_memory, (float)l->mem_limit / mb_memory), l->id());
            return nullptr;
        }
    }

    if (l->mem > l->mem_report)
    {
        l->mem_report *= 2;
        l->logger()->logstring(true, moon::LogLevel::Warn,
                               moon::format("%s Memory warning %.2f M", l->name().data(), (float)l->mem / mb_memory), l->id());
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

lua_service::lua_service()
    : lua_(sol::default_at_panic, lalloc, this)
{
}

lua_service::~lua_service()
{
    logger()->logstring(true, moon::LogLevel::Info, moon::format("[WORKER %u] destroy service [%s] ", worker_->id(), name().data()), id());
}

void lua_service::set_callback(sol_function_t f)
{
    dispatch_ = f;
}

bool lua_service::init(std::string_view config)
{
    try
    {
        service_config_parser<lua_service> conf;
        MOON_CHECK(conf.parse(this, config), "lua service init failed: parse config failed.");
        auto luafile = conf.get_value<std::string>("file");
        MOON_CHECK(!luafile.empty(), "lua service init failed: config does not provide lua file.");
        mem_limit = static_cast<size_t>(conf.get_value<int64_t>("memlimit"));

        lua_.stop_gc();

        lua_.open_libraries();

        lua_.change_gc_mode_generational(0, 0);

        lua_.add_package_loader(custom_package_loader);

        sol::table module = lua_.create_table();
        lua_bind lua_bind(module);
        lua_bind.bind_service(this)
            .bind_log(this)
            .bind_util()
            .bind_timer(this)
            .bind_socket(this);

        lua_bind::registerlib(lua_.lua_state(), "mooncore", module);

        open_custom_libraries(lua_.lua_state());

        lua_.script(router_->get_env("CPATH"));
        lua_.script(router_->get_env("PATH"));

        sol::load_result fx = lua_.load_file(luafile);
        if (!fx.valid())
        {
            sol::error err = fx;
            MOON_CHECK(false, moon::format("lua_service::init load failed: %s.", err.what()));
        }
        else
        {
            lua_json_decode(lua_.lua_state(), config.data(), config.size());//push table
            sol::table param = sol::stack::get<sol::table>(lua_.lua_state(), -1);//save table in LUA_REGISTRYINDEX
            lua_pop(lua_.lua_state(), 1);//pop table
            sol::protected_function_result call_result = fx(param);

            if (!call_result.valid())
            {
                sol::error err = call_result;
                MOON_CHECK(false, moon::format("lua_service::init run failed: %s.", err.what()));
            }
        }

        if (unique())
        {
            MOON_CHECK(router_->set_unique_service(name(), id()), moon::format("lua service init failed: unique service name %s repeated.", name().data()).data());
        }

        logger()->logstring(true, moon::LogLevel::Info, moon::format("[WORKER %u] new service [%s]", worker_->id(), name().data()), id());
        ok_ = true;

        lua_.restart_gc();
    }
    catch (const std::exception &e)
    {
        CONSOLE_ERROR(logger(), "lua_service::init config:\n%s. \n%s.", config.data(), e.what());
    }
    return ok_;
}

void lua_service::dispatch(message *msg)
{
    if (!ok())
        return;

    MOON_ASSERT(dispatch_.valid(), "should initialize callbacks first.")

    try
    {
        auto result = dispatch_((void*)msg, msg->type());
        if (!result.valid())
        {
            sol::error err = result;
            if (msg->sessionid() >= 0 || msg->receiver() == 0) //socket mesage receiver==0
            {
                logger()->logstring(true, moon::LogLevel::Error, moon::format("%s dispatch:\n%s", name().data(), err.what()), id());
            }
            else
            {
                msg->set_sessionid(-msg->sessionid());
                router_->response(msg->sender(), "lua_service::dispatch "sv, err.what(), msg->sessionid(), PTYPE_ERROR);
            }
        }
    }
    catch (const std::exception &e)
    {
        CONSOLE_ERROR(logger(), "lua_service::dispatch:\n%s\n", e.what());
        quit();
    }
}

