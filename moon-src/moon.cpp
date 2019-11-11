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

int main(int argc, char*argv[])
{
    using namespace moon;

    register_signal();

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

            moon::server_config_manger& scfg = moon::server_config_manger::instance();
            if (!service_file.empty())
            {
                std::string clibdir;
                {
                    auto dir = directory::working_directory;
                    clibdir = dir.append("clib").string();
                    moon::replace(clibdir, "\\", "/");
                }

                std::string lualibdir;
                {
                    auto dir = directory::working_directory;
                    lualibdir = dir.append("lualib").string();
                    moon::replace(lualibdir, "\\", "/");
                }

                scfg.parse(moon::format(R"([{"sid":1,"name":"test_#sid","thread":1,"cpath":["../clib"],"path":["../lualib"],"services":[{"name":"%s","file":"%s","cpath":["%s"],"path":["%s"]}]}]
                )", fs::path(service_file).stem().string().data()
                    , fs::path(service_file).filename().string().data()
                    , clibdir.data()
                    , lualibdir.data()), sid);

                printf("use clib search path: %s\n", clibdir.data());
                printf("use lualib search path: %s\n", clibdir.data());

                fs::current_path(fs::absolute(fs::path(service_file)).parent_path());
                directory::working_directory = fs::current_path();
            }
            else
            {
                MOON_CHECK(directory::exists(conf)
                    , moon::format("can not found default config file: '%s'.", conf.data()).data());
                MOON_CHECK(scfg.parse(moon::file::read_all(conf, std::ios::binary | std::ios::in), sid), "failed");
                fs::current_path(fs::absolute(fs::path(conf)).parent_path());
                directory::working_directory = fs::current_path();
            }

            auto c = scfg.find(sid);
            MOON_CHECK(nullptr != c, moon::format("config for sid=%d not found.", sid));

            router_->set_env("SID", std::to_string(c->sid));
            router_->set_env("SERVER_NAME", c->name);
            router_->set_env("INNER_HOST", c->inner_host);
            router_->set_env("OUTER_HOST", c->outer_host);
            router_->set_env("THREAD_NUM", std::to_string(c->thread));
            router_->set_env("CONFIG", scfg.config());

            server_->init(static_cast<uint8_t>(c->thread), c->log);
            server_->logger()->set_level(c->loglevel);

            for (auto&s : c->services)
            {
                router_->new_service(s.type, s.config, s.unique, s.threadid, 0, 0);
            }

            //wait all configured service is created
            while ((server_->get_state() == moon::state::init)
                && server_->service_count() < c->services.size())
            {
                std::this_thread::yield();
            }

            // then call services's start
            server_->run();
        }
        catch (std::exception& e)
        {
            printf("ERROR:%s\n", e.what());
        }
    }
    return 0;
}

