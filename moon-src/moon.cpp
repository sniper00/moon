#include <csignal>
#include "common/directory.hpp"
#include "common/string.hpp"
#include "common/file.hpp"
#include "common/lua_utility.hpp"
#include "server.h"
#include "services/lua_service.h"

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
        svr->stop(dwCtrlType);
        return TRUE;
    case CTRL_CLOSE_EVENT:
    case CTRL_SHUTDOWN_EVENT:
    case CTRL_LOGOFF_EVENT://atmost 10 second,will force closed by system
        svr->stop(dwCtrlType);
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

#ifdef MOON_ENABLE_MIMALLOC
#include "mimalloc.h"
void print_mem_stats()
{
    std::string stats;
    stats.append("mimalloc memory stats:\n");
    auto fn = [](const char* msg, void* arg)
    {
        auto p = static_cast<std::string*>(arg);
        p->append(msg);
    };

    mi_collect(true);
    mi_stats_print_out(fn, &stats);
    CONSOLE_INFO(stats.data());
}
#else
void print_mem_stats()
{
}
#endif

static void usage(void) {
    std::cout << "Usage:\n";
    std::cout << "        moon script [args]\n";
    std::cout << "Examples:\n";
    std::cout << "        moon main.lua hello\n";
}

int main(int argc, char* argv[])
{
    using namespace moon;

    int exitcode = -1;

    time::timezone();

    register_signal(argc, argv);

#ifdef LUA_CACHELIB
    luaL_initcodecache();
#endif
    try
    {
        uint32_t thread_count = std::thread::hardware_concurrency();
        bool enable_stdout = true;
        std::string logfile;
        std::string bootstrap;
        std::string loglevel;

        int argn = 1;
        if (argc <= argn)
        {
            usage();
            return exitcode;
        }
        bootstrap = argv[argn++];

        if (fs::path(bootstrap).extension() != ".lua")
        {
            usage();
            return exitcode;
        }

        std::string arg = "return {";
        for (int i = argn; i < argc; ++i)
        {
            arg.append("'");
            arg.append(argv[i]);
            arg.append("',");
        }
        arg.append("}");

        std::shared_ptr<server> server_ = std::make_shared<server>();
        wk_server = server_;

        if(file::read_all(bootstrap, std::ios::in).find("_G[\"__init__\"]") != std::string::npos)
        {
            std::unique_ptr<lua_State, moon::state_deleter> lua_{ luaL_newstate() };
            lua_State* L = lua_.get();
            luaL_openlibs(L);
            lua_pushboolean(L, true);
            lua_setglobal(L, "__init__");

            lua_pushcfunction(L, traceback);
            assert(lua_gettop(L) == 1);

            int r = luaL_loadfile(L, bootstrap.data());
            MOON_CHECK(r == LUA_OK, moon::format("loadfile %s", lua_tostring(L, -1)));

            r = luaL_dostring(L, arg.data());
            MOON_CHECK(r == LUA_OK, moon::format("%s", lua_tostring(L, -1)));
            r = lua_pcall(L, 1, 1, 1);
            MOON_CHECK(r == LUA_OK, moon::format("%s", lua_tostring(L, -1)));
            MOON_CHECK(lua_type(L, -1) == LUA_TTABLE, "init must return conf table");

            thread_count = lua_opt_field<uint32_t>(L, -1, "thread", thread_count);
            logfile = lua_opt_field<std::string>(L, -1, "logfile");
            enable_stdout = lua_opt_field<bool>(L, -1, "enable_stdout", enable_stdout);
            loglevel = lua_opt_field<std::string>(L, -1, "loglevel", loglevel);
            std::string path = lua_opt_field<std::string>(L, -1, "path", "");
            if(!path.empty()){
                path = moon::format("package.path='%s;'..package.path;", path.data());
                server_->set_env("PATH", path);
            }
        }

        server_->register_service("lua", []()->service_ptr_t {
            return std::make_unique<lua_service>();
            });

#if TARGET_PLATFORM == PLATFORM_WINDOWS
        server_->set_env("LUA_CPATH_EXT", "/?.dll;");
#else
        server_->set_env("LUA_CPATH_EXT", "/?.so;");
#endif

        if(!server_->get_env("PATH"))
        {
            // By default, lualib and service directories are added to the lua search path
            auto search_path = fs::absolute(fs::current_path());
            if (!fs::exists(search_path / "lualib"))
                search_path = fs::absolute(directory::module_path());
            MOON_CHECK(fs::exists(search_path / "lualib"), "can not find moon lualib path.");
            auto strpath = search_path.string();
            moon::replace(strpath, "\\", "/");
            server_->set_env("PATH", moon::format("package.path='%s/lualib/?.lua;%s/service/?.lua;'..package.path;", strpath.data(), strpath.data()));
        }

        //Change the working directory to the directory where the opened file is located.
        fs::current_path(fs::absolute(fs::path(bootstrap)).parent_path());
        directory::working_directory = fs::current_path();

        server_->set_env("ARG", arg);
        server_->set_env("THREAD_NUM", std::to_string(thread_count));

        log::instance().set_enable_console(enable_stdout);
        log::instance().set_level(loglevel);
        log::instance().init(logfile);

        server_->init(thread_count);

        std::unique_ptr<moon::service_conf> conf = std::make_unique<moon::service_conf>();
        conf->type = "lua";
        conf->name = "bootstrap";
        conf->source = fs::path(bootstrap).filename().string();
        conf->memlimit = std::numeric_limits<ssize_t>::max();
        auto path = server_->get_env("PATH");
        if (path)
            conf->params.append(*path);
        conf->params.append("return {}");
        server_->new_service(std::move(conf));
        server_->set_unique_service("bootstrap", BOOTSTRAP_ADDR);

        exitcode = server_->run();
    }
    catch (const std::exception& e)
    {
        if (!log::instance().is_ready()) {
            log::instance().init("");
        }
        exitcode = -1;
        CONSOLE_ERROR("ERROR:%s", e.what());
    }
    CONSOLE_INFO("STOP");
    while(log::instance().size()>0)
        thread_sleep(10);
    print_mem_stats();
    log::instance().wait();
    return exitcode;
}

#define REGISTER_CUSTOM_LIBRARY(name, lua_c_fn)\
            int lua_c_fn(lua_State*);\
            luaL_requiref(L, name, lua_c_fn, 0);\
            lua_pop(L, 1);  /* remove lib */\


extern "C" {
void open_custom_libs(lua_State* L)
{

#ifdef LUA_CACHELIB
    REGISTER_CUSTOM_LIBRARY("codecache", luaopen_cache);
#endif

    //core
    REGISTER_CUSTOM_LIBRARY("moon.core", luaopen_moon_core);
    REGISTER_CUSTOM_LIBRARY("asio.core", luaopen_asio_core);
    REGISTER_CUSTOM_LIBRARY("sharetable.core", luaopen_sharetable_core);
    REGISTER_CUSTOM_LIBRARY("socket.core", luaopen_socket_core);
    REGISTER_CUSTOM_LIBRARY("http.core", luaopen_http_core);
    REGISTER_CUSTOM_LIBRARY("fs", luaopen_fs);
    REGISTER_CUSTOM_LIBRARY("seri", luaopen_serialize);
    REGISTER_CUSTOM_LIBRARY("json", luaopen_json);
    REGISTER_CUSTOM_LIBRARY("buffer", luaopen_buffer);
    REGISTER_CUSTOM_LIBRARY("coroutine.profile", luaopen_coroutine_profile);

    //custom
    REGISTER_CUSTOM_LIBRARY("pb", luaopen_pb);
    REGISTER_CUSTOM_LIBRARY("crypt", luaopen_crypt);
    REGISTER_CUSTOM_LIBRARY("aoi", luaopen_aoi);
    REGISTER_CUSTOM_LIBRARY("clonefunc", luaopen_clonefunc);
    REGISTER_CUSTOM_LIBRARY("random", luaopen_random);
    REGISTER_CUSTOM_LIBRARY("zset", luaopen_zset);
    REGISTER_CUSTOM_LIBRARY("kcp.core", luaopen_kcp_core);
    REGISTER_CUSTOM_LIBRARY("bson", luaopen_bson);
    REGISTER_CUSTOM_LIBRARY("mongo.driver", luaopen_mongo_driver);
    REGISTER_CUSTOM_LIBRARY("navmesh", luaopen_navmesh);
    REGISTER_CUSTOM_LIBRARY("uuid", luaopen_uuid);
    REGISTER_CUSTOM_LIBRARY("schema", luaopen_schema);
}
}

