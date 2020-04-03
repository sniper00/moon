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

    const char* msg = nullptr;
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
    write(1, msg, strlen(msg));
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
        REGISTER_CUSTOM_LIBRARY("fs", luaopen_fs);
        REGISTER_CUSTOM_LIBRARY("http", luaopen_http);
        REGISTER_CUSTOM_LIBRARY("seri", luaopen_serialize);
        REGISTER_CUSTOM_LIBRARY("json", luaopen_rapidjson);
        //custom
        REGISTER_CUSTOM_LIBRARY("crypt", luaopen_crypt);
        REGISTER_CUSTOM_LIBRARY("aoi", luaopen_aoi);
    }

    int luaopen_protobuf_c(lua_State* L);
    int luaopen_sharetable_core(lua_State* L);
    int luaopen_socket_core(lua_State* L);

    //if register lua c module, name like a.b.c, use this
    int custom_package_loader(lua_State* L)
    {
        std::string_view path = sol::stack::get<std::string_view>(L, 1);
        if (path == "protobuf.c"sv)
        {
            sol::stack::push(L, luaopen_protobuf_c);
        }
        else if (path == "sharetable.core"sv)
        {
            sol::stack::push(L, luaopen_sharetable_core);
        }
        else if (path == "socket.core"sv)
        {
            sol::stack::push(L, luaopen_socket_core);
        }
        else
        {
            sol::stack::push(L, "This is not the module you're looking for!");
        }
        return 1;
    }
}


int main(int argc, char* argv[])
{
    using namespace moon;

    register_signal(argc, argv);

    luaL_initcodecache();
    {
        try
        {
            directory::working_directory = directory::current_directory();

            std::string conf = "config.json";//default config
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
                auto filepath = directory::find_file(directory::module_path().string(), "moon.lua");
                if (filepath.empty())
                {
                    filepath = directory::find_file(directory::working_directory.string(), "moon.lua");
                }
                std::string clibdir;
                std::string lualibdir;
                if (!filepath.empty())
                {
                    clibdir = fs::path(filepath).parent_path().append("clib").string();
                    moon::replace(clibdir, "\\", "/");
                    lualibdir = fs::path(filepath).parent_path().string();
                    moon::replace(lualibdir, "\\", "/");
                }

                MOON_CHECK(!lualibdir.empty(), "can not found moon 'lualib' path.");

                std::string_view fmt = R"(
                        local json = require("json")
                        local set_env = _G.set_env
                        local get_env = _G.get_env
                        local new_service = _G.new_service
                        local path = "%s/?.lua;"
                        local cpath = "%s"..get_env("LUA_CPATH_EXT")
                        set_env("PATH", path)
                        set_env("CPATH", cpath)
                        new_service("lua", json.encode({name= "test",file = "%s"}), true, 0, 0 ,0 )
                        return 1;
                    )";

                smgr.parse(R"([{"sid":1,"name":"test_#sid","bootstrap":"@"}])", sid);

                bootstrap = moon::format(fmt.data(),
                    lualibdir.data(), clibdir.data(),
                    fs::path(service_file).filename().string().data());

                printf("use clib search path: %s\n", clibdir.data());
                printf("use lualib search path: %s\n", clibdir.data());

                fs::current_path(fs::absolute(fs::path(service_file)).parent_path());
                directory::working_directory = fs::current_path();
            }
            else
            {
                MOON_CHECK(directory::exists(conf)
                    , moon::format("can not found default config file: '%s'.", conf.data()).data());
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

            server_->init(static_cast<uint8_t>(c->thread), c->log);
            server_->logger()->set_level(c->loglevel);

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

