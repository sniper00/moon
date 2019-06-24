#include "lua_bind.h"
#include "common/time.hpp"
#include "common/timer.hpp"
#include "common/directory.hpp"
#include "common/buffer_writer.hpp"
#include "common/hash.hpp"
#include "common/http_util.hpp"
#include "common/log.hpp"
#include "common/sha1.hpp"
#include "message.hpp"
#include "server.h"
#include "worker.h"
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

static int unpack_cluster_header(lua_State* L)
{
    auto buf = reinterpret_cast<buffer*>(lua_touserdata(L, 1));
    uint16_t dlen = 0;
    buf->read(&dlen, 0, 1);
    int hlen = static_cast<int>(buf->size() - dlen);
    buf->seek(dlen);
    auto n = lua_serialize::do_unpack(L,buf->data(),buf->size());
    buf->seek(-dlen);
    buf->offset_writepos(-hlen);
    return n;
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
    auto serviceid = (uint32_t)lua_tointeger(L, lua_upvalueindex(2));
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
    logger->logstring(true, moon::LogLevel::Info, moon::string_view_t{ buf.data(), buf.size() }, serviceid);
    lua_pop(L, 1);  /* pop tostring */
    return 0;
}

static void register_lua_print(sol::table& lua, moon::log* logger,uint32_t serviceid)
{
    lua_pushlightuserdata(lua.lua_state(), logger);
    lua_pushinteger(lua.lua_state(), serviceid);
    lua_pushcclosure(lua.lua_state(), my_lua_print, 2);
    lua_setglobal(lua.lua_state(), "print");
}

static void lua_extend_library(lua_State* L, lua_CFunction f, const char* gname, const char* name)
{
    lua_getglobal(L, gname);
    lua_pushcfunction(L, f);
    lua_setfield(L, -2, name);
    lua_pop(L, 1); /* pop gname table */
    assert(lua_gettop(L) == 0);
}

static void sol_extend_library(sol::table t, lua_CFunction f, const char* name, const std::function<int(lua_State* L)>& uv = nullptr)
{
    lua_State* L = t.lua_state();
    t.push();//sol table
    int upvalue = 0;
    if (uv)
    { 
        upvalue = uv(L);
    }
    lua_pushcclosure(L, f, upvalue);
    lua_setfield(L, -2, name);
    lua_pop(L, 1); //sol table
    assert(lua_gettop(L) == 0);
}

const lua_bind & lua_bind::bind_util() const
{
    lua.set_function("second", time::second);
    lua.set_function("millsecond", time::millisecond);
    lua.set_function("microsecond", time::microsecond);
    lua.set_function("time_offset", time::offset);

    lua.set_function("sleep", [](int64_t ms) { thread_sleep(ms); });

    lua.set_function("sha1", [](std::string_view s) {
        std::string buf(sha1::sha1_context::digest_size, '\0');
        sha1::sha1_context ctx;
        sha1::init(ctx);
        sha1::update(ctx, s.data(), s.size());
        sha1::finish(ctx, buf.data());
        return buf;
    });

    sol_extend_library(lua, unpack_cluster_header, "unpack_cluster_header");

    lua_extend_library(lua.lua_state(), lua_table_new, "table", "new");
    lua_extend_library(lua.lua_state(), lua_math_clamp, "math", "clamp");
    lua_extend_library(lua.lua_state(), lua_string_hash, "string", "hash");
    lua_extend_library(lua.lua_state(), lua_string_hex, "string", "hex");

    lua.new_enum<moon::buffer_flag>("buffer_flag", {
        {"close",moon::buffer_flag::close},
        {"ws_text",moon::buffer_flag::ws_text} 
    });
    return *this;
}

const lua_bind & lua_bind::bind_log(moon::log* logger, uint32_t serviceid) const
{
    lua.set_function("LOGV", &moon::log::logstring, logger);
    register_lua_print(lua, logger, serviceid);
    return *this;
}

static void redirect(message* m, const moon::string_view_t& header, uint32_t receiver, uint8_t mtype)
{
    if (header.size() != 0)
    {
        m->set_header(header);
    }
    m->set_receiver(receiver);
    m->set_type(mtype);
}

static void resend(message* m, uint32_t sender, uint32_t receiver, const moon::string_view_t& header, int32_t sessionid, uint8_t mtype)
{
    if (header.size() != 0)
    {
        m->set_header(header);
    }
    m->set_sender(sender);
    m->set_receiver(receiver);
    m->set_type(mtype);
    m->set_sessionid(-sessionid);
}

const lua_bind & lua_bind::bind_message() const
{
    lua.new_enum<moon::buffer::seek_origin>("seek_origin", {
    {"begin",moon::buffer::seek_origin::Begin},
    {"current",moon::buffer::seek_origin::Current},
    {"end",moon::buffer::seek_origin::End}
    });

    sol::table bt = lua.create_named("buffer");

    bt.set_function("write_front", [](void* p, std::string_view s)->bool {
        auto buf = reinterpret_cast<buffer*>(p);
        return buf->write_front(s.data(), 0, s.size());
    });

    bt.set_function("write_back", [](void* p, std::string_view s){
        auto buf = reinterpret_cast<buffer*>(p);
        buf->write_back(s.data(), 0, s.size());
    });

    bt.set_function("seek", [](void* p, int offset, buffer::seek_origin s){
        auto buf = reinterpret_cast<buffer*>(p);
        buf->seek(offset, s);
    });

    bt.set_function("offset_writepos", [](void* p, int offset){
        auto buf = reinterpret_cast<buffer*>(p);
        buf->offset_writepos(offset);
    });

    //https://sol2.readthedocs.io/en/latest/functions.html?highlight=lua_CFunction
    //Note that stateless lambdas can be converted to a function pointer, 
    //so stateless lambdas similar to the form [](lua_State*) -> int { ... } will also be pushed as raw functions.
    //If you need to get the Lua state that is calling a function, use sol::this_state.
    auto f_data = [](lua_State* L)->int
    {
        auto msg = sol::stack::get<message*>(L, -1);
        lua_pushlightuserdata(L, (msg->get_buffer()->data()));
        lua_pushinteger(L, msg->size());
        return 2;
    };

    lua.new_usertype<message>("message"
        , sol::call_constructor, sol::no_constructor
        , "sender", (&message::sender)
        , "sessionid", (&message::sessionid)
        , "receiver", (&message::receiver)
        , "type", (&message::type)
        , "subtype", (&message::subtype)
        , "header", (&message::header)
        , "bytes", (&message::bytes)
        , "size", (&message::size)
        , "substr", (&message::substr)
        , "buffer", [](message* m)->void* {return m->get_buffer();}
        , "redirect", redirect
        , "resend", resend
        , "data", f_data
        );
    return *this;
}

const lua_bind& lua_bind::bind_service(lua_service* s) const
{
    auto router_ = s->get_router();
    auto server_ = s->get_server();
    auto worker_ = s->get_worker();

    lua.set("null", (void*)(router_));

    lua.set_function("name", &lua_service::name, s);
    lua.set_function("id", &lua_service::id, s);
    lua.set_function("set_cb", &lua_service::set_callback, s);
    lua.set_function("memory_use", &lua_service::memory_use, s);
    lua.set_function("make_prefab", &worker::make_prefab, worker_);
    lua.set_function("send_prefab", [worker_, s](uint32_t receiver, uint32_t cacheid, const string_view_t& header, int32_t sessionid, uint8_t type) {
        worker_->send_prefab(s->id(), receiver, cacheid, header, sessionid, type);
    });
    lua.set_function("send", &router::send, router_);
    lua.set_function("new_service", &router::new_service, router_);
    lua.set_function("remove_service", &router::remove_service, router_);
    lua.set_function("runcmd", &router::runcmd, router_);
    lua.set_function("broadcast", &router::broadcast, router_);
    lua.set_function("workernum", &router::workernum, router_);
    lua.set_function("queryservice", &router::get_unique_service, router_);
    lua.set_function("set_env", &router::set_env, router_);
    lua.set_function("get_env", &router::get_env, router_);
    lua.set_function("set_loglevel", (void(moon::log::*)(string_view_t))&log::set_level, router_->logger());
    lua.set_function("abort", &server::stop, server_);
    lua.set_function("now", &server::now, server_);
    return *this;
}

const lua_bind & lua_bind::bind_socket(lua_service* s) const
{
    auto w = s->get_worker();
    auto& sock = w->socket();

    sol::table tb = lua.create_named("socket");

    tb.set_function("listen", [&sock,s](const std::string& host, uint16_t port, uint8_t type) {
        return sock.listen(host, port, s->id(), type);
    });

    tb.set_function("accept", &moon::socket::accept,&sock);
    tb.set_function("connect", [&sock, s](const std::string& host, uint16_t port, int32_t sessionid, uint32_t owner, uint8_t type, int32_t timeout) {
        return sock.connect(host, port, s->id(), owner, type, sessionid, timeout);
    });
    tb.set_function("read", &moon::socket::read, &sock);
    tb.set_function("write", &moon::socket::write, &sock);
    tb.set_function("write_with_flag", &moon::socket::write_with_flag, &sock);
    tb.set_function("write_message", &moon::socket::write_message, &sock);
    tb.set_function("close", [&sock](uint32_t fd) {
        sock.close(fd);
    });
    tb.set_function("settimeout", &moon::socket::settimeout, &sock);
    tb.set_function("setnodelay", &moon::socket::setnodelay, &sock);
    tb.set_function("set_enable_frame", &moon::socket::set_enable_frame, &sock);
    registerlib(lua.lua_state(), "socketcore", tb);
    return *this;
}

void lua_bind::registerlib(lua_State * L, const char * name, lua_CFunction f)
{
    luaL_requiref(L, name, f, 0);
    lua_pop(L, 1); /* pop result*/
}

void lua_bind::registerlib(lua_State * L, const char * name, const sol::table& module)
{
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded"); /* get 'package.loaded' */
    module.push();
    lua_setfield(L, -2, name); /* package.loaded[name] = f */
    lua_pop(L, 2); /* pop 'package' and 'loaded' tables */
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
    module.set_function("working_directory", []() {
        return directory::working_directory.string();
    });
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
        return  fs::absolute(p).lexically_relative(directory::working_directory).string();
#else
        return  lexically_relative(fs::absolute(p), directory::working_directory).string();
#endif
    });
    return module;
}

int luaopen_fs(lua_State* L)
{
    return sol::stack::call_lua(L, 1, lua_fs);
}

static sol::table lua_http(sol::this_state L)
{
    sol::state_view lua(L);
    sol::table module = lua.create_table();

    module.set_function("parse_request", [](std::string_view data) {
        std::string_view method;
        std::string_view path;
        std::string_view query_string;
        std::string_view version;
        http::case_insensitive_multimap_view header;
        int consumed = http::request_parser::parse(data, method, path, query_string, version, header);
        return std::make_tuple(consumed, method, path, query_string, version,sol::as_table(header));
    });

    module.set_function("parse_response", [](std::string_view data) {
        std::string_view version;
        std::string_view status_code;
        http::case_insensitive_multimap_view header;
        bool ok = http::response_parser::parse(data, version, status_code, header);
        return std::make_tuple(ok, version, status_code, sol::as_table(header));
    });

    module.set_function("parse_query_string", [](const std::string& data) {
        return sol::as_table(http::query_string::parse(data));
    });

    module.set_function("create_query_string", [](sol::as_table_t<http::case_insensitive_multimap> src) {
        return http::query_string::create(src.source);
    });
    return module;
}

int luaopen_http(lua_State* L)
{
    return sol::stack::call_lua(L, 1, lua_http);
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
