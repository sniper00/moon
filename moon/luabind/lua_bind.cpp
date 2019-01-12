#include "lua_bind.h"
#include "common/time.hpp"
#include "common/timer.hpp"
#include "common/directory.hpp"
#include "common/buffer_writer.hpp"
#include "common/hash.hpp"
#include "common/http_util.hpp"
#include "common/log.hpp"
#include "components/tcp/tcp.h"
#include "message.hpp"
#include "router.h"
#include "server.h"
#include "lua_buffer.hpp"
#include "lua_serialize.hpp"

#include "services/lua_service.h"

using namespace moon;

lua_bind::lua_bind(sol::table& l)
    :lua(l)
{
}

lua_bind::~lua_bind()
{
}

const lua_bind & lua_bind::bind_timer(lua_service* s) const
{
    lua.set_function("repeated", [s](int32_t duration, int32_t times)
    {
        auto& timer = s->get_worker()->timer();
        return timer.repeat(duration, times, s->id());
    });
    lua.set_function("remove_timer", [s](timer_id_t timerid)
    {
        auto& timer = s->get_worker()->timer();
        timer.remove(timerid);
    });
    return *this;
}

static std::string make_cluster_message(string_view_t header, string_view_t data)
{
    std::string ret;
    uint16_t len = static_cast<uint16_t>(data.size());
    ret.append((const char*)&len, sizeof(len));
    ret.append(data.data(), data.size());
    ret.append(header.data(), header.size());
    return ret;
}

static void pack_cluster_message(string_view_t header, message* msg)
{
    uint16_t len = static_cast<uint16_t>(msg->size());
    msg->get_buffer()->write_front(&len, 0, 1);
    msg->get_buffer()->write_back(header.data(), 0, header.size());
}

static string_view_t unpack_cluster_message(message* msg)
{
    uint16_t len = 0;
    msg->get_buffer()->read(&len, 0, 1);
    size_t header_size = msg->size() - len;
    const char* header = msg->data() + len;
    int tmp = (int)header_size;
    msg->get_buffer()->offset_writepos(-tmp);
    return string_view_t{ header,header_size };
}

static int lua_table_new(lua_State* L)
{
    auto arrn = luaL_checkinteger(L, -2);
    auto hn = luaL_checkinteger(L, -1);
    lua_createtable(L, (int)arrn, (int)hn);
    return 1;
}

static int lua_math_clamp(lua_State* L)
{
    auto value = luaL_checknumber(L, -3);
    auto min = luaL_checknumber(L, -2);
    auto max = luaL_checknumber(L, -1);
    int b = 0;
    if (value < min || value > max)
    {
        value = (value < min)?min:max;
        b = 1;
    }
    lua_pushnumber(L, value);
    lua_pushboolean(L, b);
    return 2;
}

static int lua_string_hash(lua_State* L)
{
    size_t l;
    auto s = luaL_checklstring(L, -1, &l);
    std::string_view sv{ s,l };
    size_t hs = moon::hash_range(sv.begin(), sv.end());
    lua_pushinteger(L, hs);
    return 1;
}

static int lua_string_hex(lua_State* L)
{
    size_t l;
    auto s = luaL_checklstring(L, -1, &l);
    std::string_view sv{ s,l };
    auto res = moon::hex_string(sv);
    lua_pushlstring(L, res.data(), res.size());
    return 1;
}

static int my_lua_print(lua_State *L) {
    moon::log* logger = (moon::log*)lua_touserdata(L, lua_upvalueindex(1));
    int n = lua_gettop(L);  /* number of arguments */
    int i;
    lua_getglobal(L, "tostring");
    buffer buf;
    for (i = 1; i <= n; i++) {
        const char *s;
        size_t l;
        lua_pushvalue(L, -1);  /* function to be called */
        lua_pushvalue(L, i);   /* value to print */
        lua_call(L, 1, 1);
        s = lua_tolstring(L, -1, &l);  /* get result */
        if (s == NULL)
            return luaL_error(L, "'tostring' must return a string to 'print'");
        if (i > 1) buf.write_back("\t");
        buf.write_back(s, 0, l);
        lua_pop(L, 1);  /* pop result */
    }
    logger->logstring(true, moon::LogLevel::Info, moon::string_view_t{ buf.data(), buf.size() });
    lua_pop(L, 1);  /* pop tostring */
    return 0;
}

static void register_lua_print(sol::table& lua, moon::log* logger)
{
    lua_pushlightuserdata(lua.lua_state(), logger);
    lua_pushcclosure(lua.lua_state(), my_lua_print, 1);
    lua_setglobal(lua.lua_state(), "print");
}

static void lua_extend_library(lua_State* L, lua_CFunction f, const char* gname, const char* name)
{
    lua_getglobal(L, gname);
    lua_pushcfunction(L, f);
    lua_setfield(L, -2, name); /* package.preload[name] = f */
    lua_pop(L, 1); /* pop gname table */
}

const lua_bind & lua_bind::bind_util() const
{
    lua.set_function("second", time::second);
    lua.set_function("millsecond", time::millisecond);
    lua.set_function("microsecond", time::microsecond);
    lua.set_function("time_offset", time::offset);

    lua.set_function("sleep", [](int64_t ms) { thread_sleep(ms); });

    lua.set_function("pack_cluster", pack_cluster_message);
    lua.set_function("unpack_cluster", unpack_cluster_message);
    lua.set_function("make_cluster_message", make_cluster_message);

    lua_extend_library(lua.lua_state(), lua_table_new, "table", "new");

    lua_extend_library(lua.lua_state(), lua_math_clamp, "math", "clamp");

    lua_extend_library(lua.lua_state(), lua_string_hash, "string", "hash");
    lua_extend_library(lua.lua_state(), lua_string_hex, "string", "hex");
    return *this;
}

const lua_bind & lua_bind::bind_log(moon::log* logger) const
{
    lua.set_function("LOGV", &moon::log::logstring, logger);
    register_lua_print(lua, logger);
    return *this;
}

static void redirect_message(message* m, const moon::string_view_t& header, uint32_t receiver, uint8_t mtype)
{
    if (header.size() != 0)
    {
        m->set_header(header);
    }
    m->set_receiver(receiver);
    m->set_type(mtype);
}

static void resend(message* m, uint32_t sender, uint32_t receiver, const moon::string_view_t& header, int32_t responseid, uint8_t mtype)
{
    if (header.size() != 0)
    {
        m->set_header(header);
    }
    m->set_sender(sender);
    m->set_receiver(receiver);
    m->set_type(mtype);
    m->set_responseid(-responseid);
}

static void* message_get_buffer(message* m)
{
    return m->get_buffer();
}

const lua_bind & lua_bind::bind_message() const
{
    lua.new_usertype<message>("message"
        , sol::call_constructor, sol::no_constructor
        , "sender", (&message::sender)
        , "responseid", (&message::responseid)
        , "receiver", (&message::receiver)
        , "type", (&message::type)
        , "subtype", (&message::subtype)
        , "header", (&message::header)
        , "bytes", (&message::bytes)
        , "size", (&message::size)
        , "substr", (&message::substr)
        , "buffer", message_get_buffer
        , "redirect", (redirect_message)
        , "resend", resend
        );
    return *this;
}

const lua_bind& lua_bind::bind_service(lua_service* s) const
{
    auto router_ = s->get_router();
    auto server_ = s->get_server();

    lua.set("null", (void*)(router_));

    lua.set_function("name", &lua_service::name, s);
    lua.set_function("id", &lua_service::id, s);
    lua.set_function("send_prepare", &lua_service::send_prepare, s);
    lua.set_function("prepare", &lua_service::prepare, s);
    lua.set_function("get_tcp", &lua_service::get_tcp, s);
    lua.set_function("remove_component", &lua_service::remove, s);
    lua.set_function("set_cb", &lua_service::set_callback, s);
    lua.set_function("memory_use", &lua_service::memory_use, s);
    lua.set_function("send", &router::send, router_);
    lua.set_function("new_service", &router::new_service, router_);
    lua.set_function("remove_service", &router::remove_service, router_);
    lua.set_function("runcmd", &router::runcmd, router_);
    lua.set_function("broadcast", &router::broadcast, router_);
    lua.set_function("workernum", &router::workernum, router_);
    lua.set_function("unique_service", &router::get_unique_service, router_);
    lua.set_function("set_unique_service", &router::set_unique_service, router_);
    lua.set_function("set_env", &router::set_env, router_);
    lua.set_function("get_env", &router::get_env, router_);
    lua.set_function("set_loglevel", (void(moon::log::*)(string_view_t))&log::set_level, router_->logger());
    lua.set_function("abort", &server::stop, server_);
    lua.set_function("now", &server::now, server_);
    return *this;
}

const lua_bind & lua_bind::bind_socket() const
{
    lua.new_usertype<moon::tcp>("tcp"
        , sol::call_constructor, sol::no_constructor
        , "async_accept", (&moon::tcp::async_accept)
        , "connect", (&moon::tcp::connect)
        , "async_connect", (&moon::tcp::async_connect)
        , "listen", (&moon::tcp::listen)
        , "close", (&moon::tcp::close)
        , "read", (&moon::tcp::read)
        , "send", (&moon::tcp::send)
        , "send_then_close", (&moon::tcp::send_then_close)
        , "send_message", (&moon::tcp::send_message)
        , "settimeout", (&moon::tcp::settimeout)
        , "setnodelay", (&moon::tcp::setnodelay)
        , "set_enable_frame", (&moon::tcp::set_enable_frame)
        );
    return *this;
}

const lua_bind & lua_bind::bind_http() const
{
    lua.new_usertype<moon::http::request_parser>("http_request_parser"
        , sol::constructors<sol::types<>>()
        , "parse", (&moon::http::request_parser::parse_string)
        , "method", sol::readonly(&moon::http::request_parser::method)
        , "path", sol::readonly(&moon::http::request_parser::path)
        , "query_string", sol::readonly(&moon::http::request_parser::query_string)
        , "http_version", sol::readonly(&moon::http::request_parser::http_version)
        , "header", (&moon::http::request_parser::header)
        , "has_header", (&moon::http::request_parser::has_header)
        );

    lua.new_usertype<moon::http::response_parser>("http_response_parser"
        , sol::constructors<sol::types<>>()
        , "parse", (&moon::http::response_parser::parse_string)
        , "version", sol::readonly(&moon::http::response_parser::version)
        , "status_code", sol::readonly(&moon::http::response_parser::status_code)
        , "header", (&moon::http::response_parser::header)
        , "has_header", (&moon::http::response_parser::has_header)
        );

    return *this;
}

void lua_bind::registerlib(lua_State * L, const char * name, lua_CFunction f)
{
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload"); /* get 'package.preload' */
    lua_pushcfunction(L, f);
    lua_setfield(L, -2, name); /* package.preload[name] = f */
    lua_pop(L, 2); /* pop 'package' and 'preload' tables */
}

void lua_bind::registerlib(lua_State * L, const char * name, const sol::table& module)
{
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded"); /* get 'package.preload' */
    module.push();
    lua_setfield(L, -2, name); /* package.preload[name] = f */
    lua_pop(L, 2); /* pop 'package' and 'preload' tables */
}

static void traverse_folder(const std::string& dir, int depth, sol::protected_function func, sol::this_state s)
{
    directory::traverse_folder(dir, depth, [func, s](const fs::path& path, bool isdir)->bool {
        auto result = func(path.string(), isdir);
        if (!result.valid())
        {
            sol::error err = result;
            luaL_error(s.lua_state(), err.what());
            return false;
        }
        else
        {
            if (result.return_count())
            {
                return  ((bool)result);
            }
            return true;
        }
    });
}

inline fs::path lexically_relative(const fs::path& p, const fs::path& _Base)
{
    using namespace std::string_view_literals; // TRANSITION, VSO#571749
    constexpr std::wstring_view _Dot = L"."sv;
    constexpr std::wstring_view _Dot_dot = L".."sv;
    fs::path _Result;
    if (p.root_name() != _Base.root_name()
        || p.is_absolute() != _Base.is_absolute()
        || (!p.has_root_directory() && _Base.has_root_directory()))
    {
        return (_Result);
    }

    const fs::path::iterator _This_end = p.end();
    const fs::path::iterator _Base_begin = _Base.begin();
    const fs::path::iterator _Base_end = _Base.end();

    auto _Mismatched = std::mismatch(p.begin(), _This_end, _Base_begin, _Base_end);
    fs::path::iterator& _A_iter = _Mismatched.first;
    fs::path::iterator& _B_iter = _Mismatched.second;

    if (_A_iter == _This_end && _B_iter == _Base_end)
    {
        _Result = _Dot;
        return (_Result);
    }

    {	// Skip root-name and root-directory elements, N4727 30.11.7.5 [fs.path.itr]/4.1, 4.2
        ptrdiff_t _B_dist = std::distance(_Base_begin, _B_iter);

        const ptrdiff_t _Base_root_dist = static_cast<ptrdiff_t>(_Base.has_root_name())
            + static_cast<ptrdiff_t>(_Base.has_root_directory());

        while (_B_dist < _Base_root_dist)
        {
            ++_B_iter;
            ++_B_dist;
        }
    }

    ptrdiff_t _Num = 0;

    for (; _B_iter != _Base_end; ++_B_iter)
    {
        const fs::path& _Elem = *_B_iter;

        if (_Elem.empty())
        {	// skip empty element, N4727 30.11.7.5 [fs.path.itr]/4.4
        }
        else if (_Elem == _Dot)
        {	// skip filename elements that are dot, N4727 30.11.7.4.11 [fs.path.gen]/4.2
        }
        else if (_Elem == _Dot_dot)
        {
            --_Num;
        }
        else
        {
            ++_Num;
        }
    }

    if (_Num < 0)
    {
        return (_Result);
    }

    for (; _Num > 0; --_Num)
    {
        _Result /= _Dot_dot;
    }

    for (; _A_iter != _This_end; ++_A_iter)
    {
        _Result /= *_A_iter;
    }
    return (_Result);
}

static sol::table lua_fs(sol::this_state L)
{
    sol::state_view lua(L);
    sol::table module = lua.create_table();
    module.set_function("traverse_folder", traverse_folder);
    module.set_function("exists", directory::exists);
    module.set_function("create_directory", directory::create_directory);
    module.set_function("current_directory", directory::current_directory);
    module.set_function("parent_path", [](const moon::string_view_t& s) {
        return fs::absolute(fs::path(s)).parent_path().string();
    });

    module.set_function("filename", [](const moon::string_view_t& s) {
        return fs::path(s).filename().string();
    });

    module.set_function("extension", [](const moon::string_view_t& s) {
        return fs::path(s).extension().string();
    });

    module.set_function("root_path", [](const moon::string_view_t& s) {
        return fs::path(s).root_path().string();
    });

    //filename_without_extension
    module.set_function("stem", [](const moon::string_view_t& s) {
        return fs::path(s).stem().string();
    });

    module.set_function("relative_work_path", [](const moon::string_view_t& p) {
#if TARGET_PLATFORM == PLATFORM_WINDOWS
        return  fs::absolute(p).lexically_relative(lua_service::work_path()).string();
#else
        return  lexically_relative(fs::absolute(p), lua_service::work_path()).string();
#endif
    });
    return module;
}

int luaopen_fs(lua_State* L)
{
    return sol::stack::call_lua(L, 1, lua_fs);
}

const char* lua_traceback(lua_State * L)
{
    luaL_traceback(L, L, NULL, 1);
    auto s = lua_tostring(L, -1);
    if (nullptr != s)
    {
        return "";
    }
    return s;
}
