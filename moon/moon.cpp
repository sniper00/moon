#include <csignal>
#include "log.h"
#include "server.h"
#include "luabind/lua_bind.h"
#include "common/path.hpp"
#include "common/string.hpp"
#include "common/time.hpp"
#include "common/file.hpp"
#include "server_config.hpp"
#include "services/lua_service.h"

extern "C" {
    #include "lua53/lstring.h"
}

static moon::server*  pserver = nullptr;

#if TARGET_PLATFORM == PLATFORM_WINDOWS
static BOOL WINAPI ConsoleHandlerRoutine(DWORD dwCtrlType)
{
    if (nullptr == pserver)
    {
        return TRUE;
    }

    switch (dwCtrlType)
    {
    case CTRL_CLOSE_EVENT:
    case CTRL_SHUTDOWN_EVENT:
    case CTRL_LOGOFF_EVENT://atmost 10 second,will force closed by system
    case CTRL_C_EVENT:
        pserver->stop();
        while (pserver->workernum() > 0)
        {
            thread_sleep(10);
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
    if (nullptr == pserver)
    {
        return;
    }

    switch (signal)
    {
    case SIGTERM:
        CONSOLE_ERROR(pserver->logger(), "RECV SIGTERM SIGNAL");
        pserver->stop();
        break;
    case SIGINT:
        CONSOLE_ERROR(pserver->logger(), "RECV SIGINT SIGNAL");
        pserver->stop();
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
        sid = 1;
    }

    std::string lock_file = moon::format("%d.lock", sid);
    if (path::exist(moon::format("%d.lock", sid)))
    {
        if (!path::remove(lock_file))
        {
            printf("server sid=%d already start.\n", sid);
            return -1;
        }
        else
        {
            printf("warn : server sid=%d last run not close successfully.\n", sid);
        }
    }

    std::ofstream os(lock_file);
    if (!os.is_open())
    {
        printf("write server lock file failed\n");
        return -1;
    }

    std::string lock_content = std::to_string(sid);
    os.write(lock_content.data(), lock_content.size());

    server  server_;
    pserver = &server_;

    register_signal();

    luaS_initshr();  /* init global short string table */
    {
        sol::state lua;
        try
        {
            moon::server_config_manger scfg;
            MOON_CHECK(scfg.parse(moon::file::read_all_text("config.json"), sid), "failed");
            lua.open_libraries();
            lua.require("json", luaopen_rapidjson);

            sol::table module = lua.create_table();
            lua_bind lua_bind(module);
            lua_bind.bind_path()
                .bind_log(server_.logger());

            server_.register_service("lua", []()->service_ptr_t {
                return std::make_shared<lua_service>();
            });

            lua["package"]["loaded"]["moon_core"] = module;

#if TARGET_PLATFORM == PLATFORM_WINDOWS
            lua.script("package.cpath = './clib/?.dll;'");
#else
            lua.script("package.cpath = './clib/?.so;'");
#endif
            lua.script("package.path = './?.lua;./lualib/?.lua;'");

            auto c = scfg.find(sid);
            MOON_CHECK(nullptr != c, moon::format("config for sid=%d not found.",sid));

            server_.set_env("sid", std::to_string(c->sid));
            server_.set_env("name", c->name);
            server_.set_env("inner_host", c->inner_host);
            server_.set_env("outer_host", c->outer_host);
            server_.set_env("server_config", scfg.config());

            server_.init(c->thread, c->log);
            server_.logger()->set_level(c->loglevel);
            for (auto&s : c->services)
            {
                MOON_CHECK(0 != server_.new_service(s.type, s.unique, s.shared, s.threadid, s.config), "new_service failed");
            }

            if (!c->startup.empty())
            {
                MOON_CHECK(moon::path::extension(c->startup) == ".lua", "startup file must be lua script.");
                module.set_function("new_service", [c, &server_](const std::string& service_type, bool unique, bool shareth, int workerid, const std::string& config)->uint32_t {
                    return server_.new_service(service_type, unique, shareth, workerid, config);
                });
                lua.script_file(c->startup);
            }
            server_.run();
        }
        catch (std::exception& e)
        {
            printf("ERROR:%s\n", e.what());
            printf("LUA TRACEBACK:%s\n", lua_traceback(lua.lua_state()));
            LOG_ERROR(server_.logger(), e.what());
            LOG_ERROR(server_.logger(), lua_traceback(lua.lua_state()));
            server_.stop();
        }
    }
    luaS_exitshr();
    os.close();
    path::remove(moon::format("%d.lock", sid));
    pserver = nullptr;
    return 0;
}

