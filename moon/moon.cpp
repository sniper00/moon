#include <csignal>
#include "common/log.hpp"
#include "common/directory.hpp"
#include "common/string.hpp"
#include "common/time.hpp"
#include "common/file.hpp"
#include "server.h"
#include "luabind/lua_bind.h"
#include "server_config.hpp"
#include "services/lua_service.h"

extern "C" {
#include "lua53/lstring.h"
}

static std::weak_ptr<moon::server>  wk_server;

#if TARGET_PLATFORM == PLATFORM_WINDOWS
static BOOL WINAPI ConsoleHandlerRoutine(DWORD dwCtrlType)
{
    auto svr = wk_server.lock();
    if (nullptr == svr)
    {
        return TRUE;
    }

    switch (dwCtrlType)
    {
    case CTRL_C_EVENT:
        svr->stop();
        return TRUE;
    case CTRL_CLOSE_EVENT:
    case CTRL_SHUTDOWN_EVENT:
    case CTRL_LOGOFF_EVENT://atmost 10 second,will force closed by system
        svr->stop();
        while (!svr->stoped())
        {
            std::this_thread::yield();
        }
        return TRUE;
    default:
        break;
    }
    return FALSE;
}
#else
static void signal_handler(int signal)
{
    auto svr = wk_server.lock();
    if (nullptr == svr)
    {
        return;
    }

    switch (signal)
    {
    case SIGTERM:
        CONSOLE_ERROR(svr->logger(), "RECV SIGTERM SIGNAL");
        svr->stop();
        break;
    case SIGINT:
        CONSOLE_ERROR(svr->logger(), "RECV SIGINT SIGNAL");
        svr->stop();
        break;
    default:
        break;
    }
}
#endif

static void register_signal()
{
#if TARGET_PLATFORM == PLATFORM_WINDOWS
    SetConsoleCtrlHandler(ConsoleHandlerRoutine, TRUE);
#else
    std::signal(SIGHUP, SIG_IGN);
    std::signal(SIGQUIT, SIG_IGN);
    std::signal(SIGPIPE, SIG_IGN);
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
#endif
}

int main(int argc, char*argv[])
{
    using namespace moon;

    int32_t sid = 0;
    if (argc == 2)
    {
        sid = moon::string_convert<int32_t>(argv[1]);
    }
    else
    {
        sid = 1;//default start server 1
    }

    register_signal();

    luaS_initshr();  /* init global short string table */
    {
        sol::state lua;
        try
        {
            std::shared_ptr<server> server_ = std::make_shared<server>();
            wk_server = server_;

            auto router_ = server_->get_router();

            MOON_CHECK(directory::exists("config.json"), "can not found config file: config.json");
            moon::server_config_manger scfg;
            MOON_CHECK(scfg.parse(moon::file::read_all("config.json",std::ios::binary|std::ios::in), sid), "failed");
            lua.open_libraries();
            lua.require("json", luaopen_rapidjson);

            sol::table module = lua.create_table();
            lua_bind lua_bind(module);
            lua_bind.bind_filesystem()
                .bind_log(server_->logger());

            lua["package"]["loaded"]["moon_core"] = module;

            router_->register_service("lua", []()->service_ptr_t {
                return std::make_shared<lua_service>();
            });

#if TARGET_PLATFORM == PLATFORM_WINDOWS
            lua.script("package.cpath = './clib/?.dll;'");
#else
            lua.script("package.cpath = './clib/?.so;'");
#endif
            lua.script("package.path = './?.lua;./lualib/?.lua;'");

            auto c = scfg.find(sid);
            MOON_CHECK(nullptr != c, moon::format("config for sid=%d not found.", sid));

            router_->set_env("sid", std::to_string(c->sid));
            router_->set_env("name", c->name);
            router_->set_env("inner_host", c->inner_host);
            router_->set_env("outer_host", c->outer_host);
            router_->set_env("server_config", scfg.config());

            server_->init(static_cast<uint8_t>(c->thread), c->log);
            server_->logger()->set_level(c->loglevel);

            if (!c->startup.empty())
            {
                MOON_CHECK(fs::path(c->startup).extension() == ".lua", "startup file must be lua script.");
                module.set_function("new_service", [c, &router_](const std::string& service_type, bool unique, bool shareth, int workerid, const std::string& config)->uint32_t {
                    return  router_->new_service(service_type, unique, shareth, workerid, config);
                });
                lua.script_file(c->startup);
            }

            for (auto&s : c->services)
            {
                MOON_CHECK(0 != router_->new_service(s.type, s.unique, s.shared, s.threadid, s.config), "new_service failed");
            }
            server_->run();
        }
        catch (std::exception& e)
        {
            printf("ERROR:%s\n", e.what());
            printf("LUA TRACEBACK:%s\n", lua_traceback(lua.lua_state()));
        }
    }
    luaS_exitshr();
    return 0;
}

