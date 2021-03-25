#include <csignal>
#include "common/log.hpp"
#include "common/directory.hpp"
#include "common/string.hpp"
#include "common/time.hpp"
#include "common/file.hpp"
#include "common/lua_utility.hpp"
#include "server.h"
#include "server_config.hpp"
#include "services/lua_service.h"

extern "C" {
#include "lstring.h"
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
        svr->stop(100+ dwCtrlType);
        return TRUE;
    case CTRL_CLOSE_EVENT:
    case CTRL_SHUTDOWN_EVENT:
    case CTRL_LOGOFF_EVENT://atmost 10 second,will force closed by system
        svr->stop(100 + dwCtrlType);
        while (svr->get_state() != moon::state::stopped)
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

    std::string_view msg;
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
    svr->stop(signal);
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
            moon::requiref(L, name, lua_c_fn);\


extern "C"
{
    void open_custom_libraries(lua_State* L)
    {
        //core
#ifdef LUA_CACHELIB
        REGISTER_CUSTOM_LIBRARY("codecache", luaopen_cache);
#endif
        REGISTER_CUSTOM_LIBRARY("mooncore", luaopen_moon);
        REGISTER_CUSTOM_LIBRARY("asio", luaopen_asio);
        REGISTER_CUSTOM_LIBRARY("fs", luaopen_fs);
        REGISTER_CUSTOM_LIBRARY("http", luaopen_http);
        REGISTER_CUSTOM_LIBRARY("seri", luaopen_serialize);
        REGISTER_CUSTOM_LIBRARY("json", luaopen_json);
        REGISTER_CUSTOM_LIBRARY("buffer", luaopen_buffer);
        REGISTER_CUSTOM_LIBRARY("sharetable.core", luaopen_sharetable_core);
        REGISTER_CUSTOM_LIBRARY("socket.core", luaopen_socket_core);

        //custom
        REGISTER_CUSTOM_LIBRARY("pb", luaopen_pb);
        REGISTER_CUSTOM_LIBRARY("pb.unsafe", luaopen_pb_unsafe);
        REGISTER_CUSTOM_LIBRARY("crypt", luaopen_crypt);
        REGISTER_CUSTOM_LIBRARY("aoi", luaopen_aoi);
        REGISTER_CUSTOM_LIBRARY("clonefunc", luaopen_clonefunc);
        REGISTER_CUSTOM_LIBRARY("random", luaopen_random);
        REGISTER_CUSTOM_LIBRARY("zset", luaopen_zset);

    }
}


#ifdef MOON_ENABLE_MIMALLOC
#include "mimalloc.h"
void print_mem_stats()
{
    mi_collect(true);
    mi_stats_print(nullptr);
}
#else
void print_mem_stats()
{
}
#endif

int main(int argc, char* argv[])
{
    using namespace moon;

    time::timezone();

    register_signal(argc, argv);

#ifdef LUA_CACHELIB
    luaL_initcodecache();
#endif
    {
        try
        {
            directory::working_directory = directory::current_directory();

            bool enable_console = true;//enable console log
            int32_t node = 1;//default start server 1
            std::string conf = "config.json";//default config filename
            std::string bootstrap;

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
                    node = moon::string_convert<int32_t>(argv[++i]);
                }
                else if ((v == "-f"sv || v == "--file"sv) && !lastarg)
                {
                    bootstrap = argv[++i];
                    if (fs::path(bootstrap).extension() != ".lua")
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

#if TARGET_PLATFORM == PLATFORM_WINDOWS
            router_->set_env("LUA_CPATH_EXT", "/?.dll;");
#else
            router_->set_env("LUA_CPATH_EXT", "/?.so;");
#endif

            moon::server_config_manger smgr;

            if (!bootstrap.empty())
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

                router_->set_env("PATH", moon::format("package.path='%s/?.lua;'..package.path", lualibdir.data()));
                if (!clibdir.empty())
                {
                    router_->set_env("CPATH", moon::format("package.path='%s/%s'..package.path", clibdir.data(), router_->get_env("LUA_CPATH_EXT").data()));
                }

                printf("use clib search path: %s\n", clibdir.data());
                printf("use lualib search path: %s\n", lualibdir.data());

                moon::replace(bootstrap, "\\", "/");

                fs::path p(bootstrap);
                std::string filename = p.filename().string();
                fs::current_path(fs::absolute(p).parent_path());
                MOON_CHECK(smgr.parse(moon::format("[{\"node\":1,\"name\":\"bootstrap\",\"bootstrap\":\"%s\"}]", filename.data()), node),"pase config failed");
            }
            else
            {
                MOON_CHECK(directory::exists(conf)
                    , moon::format("can not found config file: '%s'.", conf.data()).data());
                MOON_CHECK(smgr.parse(moon::file::read_all(conf, std::ios::binary | std::ios::in), node), "failed");
                fs::current_path(fs::absolute(fs::path(conf)).parent_path());
            }

            directory::working_directory = fs::current_path();

            auto c = smgr.find(node);
            MOON_CHECK(nullptr != c, moon::format("config for node=%d not found.", node));

            router_->set_env("NODE", std::to_string(c->node));
            router_->set_env("SERVER_NAME", c->name);
            router_->set_env("THREAD_NUM", std::to_string(c->thread));
            router_->set_env("CONFIG", smgr.config());
            router_->set_env("PARAMS", c->params);

            server_->logger()->set_level(c->loglevel);
            server_->logger()->set_enable_console(enable_console);

            server_->init(c->thread, c->log);

            router_->new_service("lua", moon::format(R"({"name": "bootstrap","file":"%s"})",c->bootstrap.data()), false, 0,  0, 0);
            router_->set_unique_service("bootstrap", 0x01000001);

            server_->run();
        }
        catch (std::exception& e)
        {
            printf("ERROR:%s\n", e.what());
        }
    }
    print_mem_stats();
    return 0;
}

