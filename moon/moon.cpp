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
#include <stack>
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
        while (svr->get_state() != moon::state::exited)
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

void usage(void) {
    std::cout << "Usage:\n";
    std::cout << "        ./moon [/path/to/config.json] [options]\n";
    std::cout << "The options are:\n";
    std::cout << "        -c <config>           Server config file.(default: config.json).\n";
    std::cout << "        -r <run>              Specify ID for server to run.(default: 1).\n";
    std::cout << "        --service-file        Run server with a service use the specified lua file.\n";
}

int main(int argc, char*argv[])
{
    using namespace moon;

    std::string config = "config.json";//default config
    int32_t sid = 1;//default start server 1
    std::string service_file;

    for (int i = 1; i < argc; ++i)
    {
        bool lastarg = i == (argc - 1);
        std::string_view v{ argv[i] };
        if ((v == "-h"sv || v == "--help"sv) && !lastarg)
        {
            usage();
            return -1;
        }
        else if ((v == "-c"sv || v == "-config"sv) && !lastarg)
        {
            config = argv[++i];
        }
        else if ((v == "-r"sv || v == "-run"sv) && !lastarg)
        {
            try
            {
                sid = moon::string_convert<int32_t>(argv[++i]);
            }
            catch (std::exception& e)
            {
                std::cout << e.what()<<std::endl;
                usage();
                return -1;
            }
        }
        else if ((v == "--service-file") && !lastarg)
        {
            service_file = argv[++i];
            if (fs::path(service_file).extension() != ".lua")
            {
                std::cout << "service file must be a lua script.\n";
                usage();
                return -1;
            }
        }
        else
        {
            usage();
            return -1;
        }
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

            lua.open_libraries();

            sol::table module = lua.create_table();
            lua_bind lua_bind(module);
            lua_bind.bind_log(server_->logger());

            lua_bind::registerlib(lua.lua_state(), "json", luaopen_rapidjson);
            lua_bind::registerlib(lua.lua_state(), "fs", luaopen_fs);
            lua_bind::registerlib(lua.lua_state(), "moon_core", module);

            router_->register_service("lua", []()->service_ptr_t {
                return std::make_unique<lua_service>();
            });

#if TARGET_PLATFORM == PLATFORM_WINDOWS
            lua.script("package.cpath = './clib/?.dll;'");
#else
            lua.script("package.cpath = './clib/?.so;'");
#endif
            lua.script("package.path = './?.lua;./lualib/?.lua;'");

            if (!service_file.empty())
            {
                server_->init(1, "");
                server_->logger()->set_level("DEBUG");
                auto config_string = moon::format(R"({"name":"%s","file":"%s"})", fs::path(service_file).stem().string().data(), service_file.data());
                MOON_CHECK(router_->new_service("lua", config_string, true, 0, 0, 0), "new_service failed");
            }
            else
            {
                MOON_CHECK(directory::exists(config), moon::format("can not found config file: %s", config.data()).data());
                moon::server_config_manger& scfg = moon::server_config_manger::instance();
                MOON_CHECK(scfg.parse(moon::file::read_all(config, std::ios::binary | std::ios::in), sid), "failed");
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
                    MOON_CHECK(fs::path(c->startup).extension() == ".lua", "startup file must be a lua script.");
                    module.set_function("new_service", [&router_](const std::string& service_type, const std::string& config, bool unique, int workerid)->uint32_t {
                        return  router_->new_service(service_type, config, unique, workerid, 0, 0);
                    });
                    lua.script_file(c->startup);
                }
                size_t count = 0;
                for (auto&s : c->services)
                {
                    if (s.unique) ++count;
                    MOON_CHECK(router_->new_service(s.type, s.config, s.unique, s.threadid, 0, 0), "new_service failed");
                }
                //wait all configured unique service is created.
                while ((server_->get_state() == moon::state::ready) && router_->unique_service_size() != count);
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

