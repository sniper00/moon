#include "lua_service.h"
#include "message.hpp"
#include "router.h"
#include "common/hash.hpp"
#include "rapidjson/document.h"
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
            CONSOLE_ERROR(l->logger(), "%s Memory error current %.2f M, limit %.2f M", l->name().data(), (float)(l->mem) / (1024 * 1024), (float)l->mem_limit / (1024 * 1024));
            l->mem = mem;
            return NULL;
        }
    }

    if (l->mem > l->mem_report) {
        l->mem_report *= 2;
        CONSOLE_WARN(l->logger(), "%s Memory warning %.2f M", l->name().data(), (float)l->mem / (1024 * 1024));
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

void lua_service::runcmd(uint32_t sender, const std::string & cmd, int32_t responseid)
{
    auto params = moon::split<std::string>(cmd, ".");
    if (auto iter = commands_.find(params[2]); iter != commands_.end())
    {
        get_router()->make_response(sender, "", iter->second(params), responseid);
    }
}

const fs::path& lua_service::work_path()
{
    static auto current_path = fs::current_path();
    return current_path;
}

lua_service::lua_service()
    :error_(true)
    , lua_(sol::default_at_panic, lalloc, this)
    , cache_uuid_(0)
{
}

lua_service::~lua_service()
{
}

moon::tcp * lua_service::get_tcp(const std::string& protocol)
{
    uint8_t v = 0;
    switch (moon::chash_string(protocol))
    {
    case "default"_csh:
    {
        v = PTYPE_SOCKET;
        break;
    }
    case "websocket"_csh:
    {
        v = PTYPE_SOCKET_WS;
        break;
    }
    default:
        if (protocol.find("custom") != std::string::npos)
        {
            v = PTYPE_TEXT;
            break;
        }
        CONSOLE_ERROR(logger(), "Unsupported tcp  protocol %s.Support: 'default' 'custom' 'websocket'.", protocol.data());
        return nullptr;
    }

    auto p = get_component<moon::tcp>(protocol);
    if (nullptr == p)
    {
        p = add_component<moon::tcp>(protocol);
    }
    p->setprotocol(v);
    return ((p != nullptr) ? p.get() : nullptr);
}

uint32_t lua_service::make_cache(const moon::buffer_ptr_t & buf)
{
    auto iter = caches_.emplace(cache_uuid_++, buf);
    if (iter.second)
    {
        return iter.first->first;
    }
    return 0;
}

void lua_service::send_cache(uint32_t receiver, uint32_t cacheid, const string_view_t& header, int32_t responseid, uint8_t type) const
{
    auto iter = caches_.find(cacheid);
    if (iter == caches_.end())
    {
        CONSOLE_DEBUG(logger(), "send_cache failed, can not find cache data id %s", cacheid);
        return;
    }

    get_router()->send(id(), receiver, iter->second, header, responseid, type);
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

void lua_service::set_on_timer(sol_function_t f)
{
    timer_.set_on_timer([this, f](timer_id_t tid) {
        auto result = f(tid);
        if (!result.valid())
        {
            sol::error err = result;
            CONSOLE_ERROR(logger(), "%s", err.what());
        }
    });
}

void lua_service::set_remove_timer(sol_function_t f)
{
    timer_.set_remove_timer([this, f](timer_id_t tid) {
        auto result = f(tid);
        if (!result.valid())
        {
            sol::error err = result;
            CONSOLE_ERROR(logger(), "%s", err.what());
        }
    });
}

void lua_service::register_command(const std::string & command, sol_function_t f)
{
    auto hander = [this, command, f](const std::vector<std::string>& params) {
        auto result = f(params);
        if (!result.valid())
        {
            sol::error err = result;
            CONSOLE_ERROR(logger(), "%s", err.what());
            return std::string(err.what());
        }
        else
        {
            return result.get<std::string>();
        }
    };
    commands_.try_emplace(command, hander);
}

bool lua_service::init(const string_view_t& config)
{
    service_config<lua_service> scfg;
    if (!scfg.parse(this, config))
    {
        return false;
    }

    auto luafile = scfg.get_value<std::string>("file");
    if (luafile.empty())
    {
        CONSOLE_ERROR(logger(), "Lua service init failed,does not provide luafile config.");
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
                .bind_timer(&timer_)
                .bind_message()
                .bind_socket()
                .bind_http();

            lua_.require("fs", luaopen_fs);
            lua_.require("seri", lua_serialize::open);
            lua_.require("codecache", luaopen_cache);
            lua_.require("json", luaopen_rapidjson);

            lua_["package"]["loaded"]["moon_core"] = module;

            auto cpaths = scfg.get_value<std::vector<std::string_view>>("cpath");
            {
                cpaths.emplace_back("./clib");
                std::string strpath;
                strpath.append("package.cpath ='");
                for (auto& v : cpaths)
                {
                    strpath.append(v.data(), v.size());
                    strpath.append(LUA_CPATH_STR);
                }
                strpath.append("'..package.cpath");
                lua_.script(strpath);
            }

            auto paths = scfg.get_value<std::vector<std::string_view>>("path");
            {
                paths.emplace_back("./lualib");
                std::string strpath;
                strpath.append("package.path ='");
                for (auto& v : paths)
                {
                    strpath.append(v.data(), v.size());
                    strpath.append(LUA_PATH_STR);
                }
                strpath.append("'..package.path");
                lua_.script(strpath);
            }

            if (!directory::exists(luafile))
            {
                paths.push_back("./");
                std::string file;
                for (auto& p : paths)
                {
                    file = directory::find_file(fs::path(p).parent_path().string(), luafile);
                    if (!file.empty())
                    {
                        break;
                    }
                }
                MOON_CHECK(directory::exists(file), moon::format("luafile %s not found", luafile.data()).data());
                luafile = file;
            }

            if (auto result = lua_.script_file(luafile);!result.valid())
            {
                sol::error err = result;
                CONSOLE_ERROR(logger(), "%s", err.what());
                return false;
            }
 
            if (init_.valid())
            {
                if (auto result = init_(config); result.valid())
                {
                    error_ = (!(bool)result);
                }
                else
                {
                    error_ = true;
                    sol::error err = result;
                    CONSOLE_ERROR(logger(), "%s", err.what());
                }
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
            auto result = start_();
            if (!result.valid())
            {
                sol::error err = result;
                CONSOLE_ERROR(logger(), "%s", err.what());
            }
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
        auto result = dispatch_(msg, msg->type());
        if (!result.valid())
        {
            sol::error err = result;
            if (msg->responseid() == 0)
            {
                CONSOLE_ERROR(logger(), "lua_service::dispatch:%s", err.what());
            }
            else
            {
                msg->set_responseid(-msg->responseid());
                get_router()->make_response(msg->sender(), "lua_service::dispatch "sv, err.what(), msg->responseid(), PTYPE_ERROR);
            }
        }
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
        if (auto n = timer_.update(); n > 1000)
        {
            CONSOLE_WARN(logger(), "service(%s:%u) timer update takes too long: %" PRId64 "ms", name().data(), id(), n);
        }
        if (cache_uuid_ != 0)
        {
            cache_uuid_ = 0;
            caches_.clear();
        }
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
                auto result = exit_();
                if (!result.valid())
                {
                    sol::error err = result;
                    CONSOLE_ERROR(logger(), "%s", err.what());
                }
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
                auto result = destroy_();
                if (!result.valid())
                {
                    sol::error err = result;
                    CONSOLE_ERROR(logger(), "%s", err.what());
                }
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
    CONSOLE_ERROR(logger(), "%s %s", name().data(), (msg + backtrace).data());
    removeself(true);
    if (unique())
    {
        CONSOLE_ERROR(logger(), "unique service %s crashed, server will exit.", name().data());
        get_router()->stop_server();
    }
}

size_t lua_service::memory_use()
{
    return  mem;
}



