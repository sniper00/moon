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

    std::string_view msg = nullptr;
    switch (signal)
    {
    case SIGTERM:
        msg = "Received SIGTERM,shutdown...\n";
        break;
    case SIGINT:
        msg = "Received SIGINT,shutdown...\n";
        break;
    default:
        msg = "Received shutdown signal,shutdown...\n";
        break;
    }
    svr->stop();
    [[maybe_unused]] auto n = write(STDERR_FILENO, msg.data(), msg.size());
}
#endif

static void register_signal(int argc, char* argv[])
{
#if TARGET_PLATFORM == PLATFORM_WINDOWS
    SetConsoleCtrlHandler(ConsoleHandlerRoutine, TRUE);
    std::string str;
    for (int i = 0; i < argc; ++i)
    {
        str.append(argv[i]);
        if (i == 0)
        {
            str.append("(PID: ");
            str.append(std::to_string(GetCurrentProcessId()));
            str.append(")");
        }
        str.append(" ");
    }
    SetConsoleTitle(str.data());
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
    std::cout << "        moon [-c filename] [-r server-id] [-f lua-filename]\n";
    std::cout << "The options are:\n";
    std::cout << "        -c          set configuration file (default: config.json). will change current working directory to configuration file's path.\n";
    std::cout << "        -r          set server id to run (default: 1).\n";
    std::cout << "        -f          run a lua file. will change current working directory to lua file's path.\n";
    std::cout << "Examples:\n";
    std::cout << "        moon -c config.json\n";
    std::cout << "        moon -c config.json -r 1\n";
    std::cout << "        moon -r 1\n";
    std::cout << "        moon -f aoi_example.lua\n";
}

#define REGISTER_CUSTOM_LIBRARY(name, lua_c_fn)\
            int lua_c_fn(lua_State* L);\
\
            lua_bind::registerlib(L, name, lua_c_fn);\


extern "C"
{
    void open_custom_libraries(lua_State* L)
    {
        //core
#ifdef LUA_CACHELIB
        REGISTER_CUSTOM_LIBRARY("codecache", luaopen_cache);
#endif
        REGISTER_CUSTOM_LIBRARY("fs", luaopen_fs);
        REGISTER_CUSTOM_LIBRARY("http", luaopen_http);
        REGISTER_CUSTOM_LIBRARY("seri", luaopen_serialize);
        REGISTER_CUSTOM_LIBRARY("json", luaopen_json);
        REGISTER_CUSTOM_LIBRARY("buffer", luaopen_buffer);
        //custom
        REGISTER_CUSTOM_LIBRARY("pb", luaopen_pb);
        REGISTER_CUSTOM_LIBRARY("crypt", luaopen_crypt);
        REGISTER_CUSTOM_LIBRARY("aoi", luaopen_aoi);
        REGISTER_CUSTOM_LIBRARY("clonefunc", luaopen_clonefunc);
    }

    int luaopen_sharetable_core(lua_State* L);
    int luaopen_socket_core(lua_State* L);
    int luaopen_pb_unsafe(lua_State* L);

    //if register lua c module, name like a.b.c, use this
    int custom_package_loader(lua_State* L)
    {
        typedef std::unordered_map<std::string_view, lua_CFunction> pkg_map;
        static const pkg_map pkgs{
            { "sharetable.core", luaopen_sharetable_core },
            { "socket.core", luaopen_socket_core },
            { "pb.unsafe", luaopen_pb_unsafe }
        };

        std::string_view path = sol::stack::get<std::string_view>(L, 1);
        if (auto iter = pkgs.find(path); iter != pkgs.end())
        {
            sol::stack::push(L, iter->second);
        }
        else
        {
            sol::stack::push(L, moon::format("Can not find module %s!", std::string(path).data()));
        }
        return 1;
    }
}


int main(int argc, char* argv[])
{
    using namespace moon;

    register_signal(argc, argv);

#ifdef LUA_CACHELIB
    luaL_initcodecache();
#endif
    {
        try
        {
            directory::working_directory = directory::current_directory();

            std::string conf = "config.json";//default config
            int32_t sid = 1;//default start server 1
            bool enable_console = true;
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
                else if ((v == "-c"sv || v == "--config"sv) && !lastarg)
                {
                    conf = argv[++i];
                    if (!fs::exists(conf))
                    {
                        usage();
                        return -1;
                    }
                }
                else if ((v == "-r"sv || v == "--run"sv) && !lastarg)
                {
                    sid = moon::string_convert<int32_t>(argv[++i]);
                }
                else if ((v == "-f"sv || v == "--file"sv) && !lastarg)
                {
                    service_file = argv[++i];
                    if (fs::path(service_file).extension() != ".lua")
                    {
                        std::cout << "service file must be a lua script.\n";
                        usage();
                        return -1;
                    }
                }
                else if (v == "--hide"sv)
                {
                    enable_console = false;
                }
                else
                {
                    usage();
                    return -1;
                }
            }

            std::shared_ptr<server> server_ = std::make_shared<server>();
            wk_server = server_;

            auto router_ = server_->get_router();

            router_->register_service("lua", []()->service_ptr_t {
                return std::make_unique<lua_service>();
                });

            moon::server_config_manger smgr;

            std::string bootstrap;

            if (!service_file.empty())
            {
                std::string searchdir = directory::module_path().string();
                searchdir += ";";
                searchdir += directory::working_directory.string();

                auto lualibdir = directory::find(searchdir, "moon.lua");
                MOON_CHECK(!lualibdir.empty(), "can not found moon 'lualib' path.");
                lualibdir = fs::path(lualibdir).parent_path().string();
                moon::replace(lualibdir, "\\", "/");

                auto clibdir = directory::find(searchdir, "clib");
                moon::replace(clibdir, "\\", "/");

                std::string_view fmt = R"(
                        local json = require("json")
                        local set_env = _G.set_env
                        local get_env = _G.get_env
                        local new_service = _G.new_service
                        local path = "%s/?.lua;"
                        local cpath = "%s"..get_env("LUA_CPATH_EXT")
                        set_env("PATH", path)
                        set_env("CPATH", cpath)
                        new_service("lua", json.encode({name= "%s",file = "%s"}), true, 0, 0 ,0 )
                        return 1;
                    )";

                smgr.parse(R"([{"sid":1,"name":"test_#sid","bootstrap":"@"}])", sid);

                bootstrap = moon::format(
                    fmt.data(),
                    lualibdir.data(),
                    clibdir.data(),
                    fs::path(service_file).stem().string().data(),
                    fs::path(service_file).filename().string().data()
                );

                printf("use clib search path: %s\n", clibdir.data());
                printf("use lualib search path: %s\n", lualibdir.data());

                fs::current_path(fs::absolute(fs::path(service_file)).parent_path());
                directory::working_directory = fs::current_path();
            }
            else
            {
                MOON_CHECK(directory::exists(conf)
                    , moon::format("can not found config file: '%s'.", conf.data()).data());
                MOON_CHECK(smgr.parse(moon::file::read_all(conf, std::ios::binary | std::ios::in), sid), "failed");
                fs::current_path(fs::absolute(fs::path(conf)).parent_path());
                directory::working_directory = fs::current_path();
            }

            auto c = smgr.find(sid);
            MOON_CHECK(nullptr != c, moon::format("config for sid=%d not found.", sid));

            router_->set_env("SID", std::to_string(c->sid));
            router_->set_env("SERVER_NAME", c->name);
            router_->set_env("THREAD_NUM", std::to_string(c->thread));
            router_->set_env("CONFIG", smgr.config());

            router_->set_env("PARAMS", c->params);

#if TARGET_PLATFORM == PLATFORM_WINDOWS
            router_->set_env("LUA_CPATH_EXT", "/?.dll;");
#else
            router_->set_env("LUA_CPATH_EXT", "/?.so;");
#endif
            server_->logger()->set_level(c->loglevel);
            server_->logger()->set_enable_console(enable_console);

            server_->init(c->thread, c->log);

            sol::state lua;
            lua.open_libraries();

            lua.add_package_loader(custom_package_loader);

            open_custom_libraries(lua.lua_state());

            lua.set_function("new_service", &router::new_service, router_);
            lua.set_function("set_env", &router::set_env, router_);
            lua.set_function("get_env", &router::get_env, router_);

            if (c->bootstrap[0] != '@')
            {
                MOON_CHECK(directory::exists(c->bootstrap)
                    , moon::format("can not found bootstrap file: '%s'.", c->bootstrap.data()).data());
                bootstrap = moon::file::read_all(c->bootstrap, std::ios::binary | std::ios::in);
            }

            auto res = lua.script(bootstrap);
            if (!res.valid())
            {
                sol::error err = res;
                printf("bootstrap error %s\n", err.what());
                return -1;
            }

            size_t count = res;
            server_->run(count);
        }
        catch (std::exception& e)
        {
            printf("ERROR:%s\n", e.what());
        }
    }
    return 0;
}

