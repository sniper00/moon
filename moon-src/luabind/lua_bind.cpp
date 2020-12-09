#include "lua_bind.h"
#include "common/time.hpp"
#include "common/timer.hpp"
#include "common/hash.hpp"
#include "common/log.hpp"
#include "common/sha1.hpp"
#include "common/md5.hpp"
#include "common/byte_convert.hpp"
#include "common/lua_utility.hpp"
#include "message.hpp"
#include "server.h"
#include "worker.h"
#include "services/lua_service.h"

//https://sol2.readthedocs.io/en/latest/functions.html?highlight=lua_CFunction
//Note that stateless lambdas can be converted to a function pointer, 
//so stateless lambdas similar to the form [](lua_State*) -> int { ... } will also be pushed as raw functions.
//If you need to get the Lua state that is calling a function, use sol::this_state.

using namespace moon;

template <typename Handler>
inline static bool sol_lua_check(sol::types<buffer_ptr_t>, lua_State* L, int index, Handler&& handler, sol::stack::record& tracking) {
    sol::type t = sol::type_of(L, index);
    switch (t)
    {
    case sol::type::lua_nil:
    case sol::type::lightuserdata:
    case sol::type::string:
        tracking.use(1);
        return true;
    default:
        handler(L, index, sol::type::lightuserdata, t, "expected nil or a  lightuserdata(buffer*) or a string");
        return false;
    }
}

inline static buffer_ptr_t sol_lua_get(sol::types<buffer_ptr_t>, lua_State* L, int index, sol::stack::record& tracking) {
    tracking.use(1);
    sol::type t = sol::type_of(L, index);
    switch (t)
    {
    case sol::type::lua_nil:
    {
        return nullptr;
    }
    case sol::type::string:
    {
        std::size_t len;
        auto str = lua_tolstring(L, index, &len);
        auto buf = moon::message::create_buffer(len);
        buf->write_back(str, len);
        return buf;
    }
    case sol::type::lightuserdata:
    {
        moon::buffer* p = static_cast<moon::buffer*>(lua_touserdata(L, index));
        return moon::buffer_ptr_t(p);
    }
    default:
        break;
    }
    return nullptr;
}

inline static int sol_lua_push(sol::types<buffer_ptr_t>, lua_State* L, const moon::buffer_ptr_t& buf) {
    if (nullptr == buf)
    {
        return sol::stack::push(L, sol::lua_nil);
    }
    return sol::stack::push(L, (const char*)buf->data(), buf->size());
}

lua_bind::lua_bind(sol::table& l)
    :lua(l)
{
}

lua_bind::~lua_bind()
{
}

const lua_bind& lua_bind::bind_timer(lua_service* s) const
{
    lua.set_function("repeated", [s](int32_t interval, int32_t times)
        {
            return s->get_worker()->repeat(interval, times, s->id());
        });

    lua.set_function("remove_timer", [s](moon::timer_t timerid)
        {
            s->get_worker()->remove_timer(timerid);
        });
    return *this;
}

static int lua_table_new(lua_State* L)
{
    auto arrn = luaL_checkinteger(L, -2);
    auto hn = luaL_checkinteger(L, -1);
    lua_createtable(L, (int)arrn, (int)hn);
    return 1;
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

static int console_log(lua_State* L) {
    lua_service* service = (lua_service*)lua_touserdata(L, lua_upvalueindex(1));
    auto level = (moon::LogLevel)lua_tointeger(L, lua_upvalueindex(2));

    int n = lua_gettop(L);  /* number of arguments */
    int i;
    buffer buf;
    for (i = 1; i <= n; i++) {  /* for each argument */
        size_t l;
        const char* s = luaL_tolstring(L, i, &l);  /* convert it to string */
        if (i > 1)  /* not the first element? */
            buf.write_back("\t", 1);  /* add a tab before it */
        buf.write_back(s, l);  /* print it */
        lua_pop(L, 1);  /* pop result */
    }

    lua_Debug ar;
    if (lua_getstack(L, 1, &ar))
    {
        if (lua_getinfo(L, "Sl", &ar))
        {
            buf.write_back("\t(", 2);
            if (ar.source != nullptr && ar.source[0] != '\0')
                buf.write_back(ar.source + 1, std::strlen(ar.source) - 1);
            buf.write_back(":", 1);
            auto line = std::to_string(ar.currentline);
            buf.write_back(line.data(), line.size());
            buf.write_back(")", 1);
        }
    }

    service->logger()->logstring(true, level, std::string_view{ buf.data(), buf.size() }, service->id());
    return 0;
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

const lua_bind& lua_bind::bind_util() const
{
    lua.set_function("second", time::second);
    lua.set_function("millsecond", time::millisecond);
    lua.set_function("microsecond", time::microsecond);

    lua.set_function("sha1", [](std::string_view s) {
        std::string buf(sha1::sha1_context::digest_size, '\0');
        sha1::sha1_context ctx;
        sha1::init(ctx);
        sha1::update(ctx, s.data(), s.size());
        sha1::finish(ctx, buf.data());
        return buf;
        });

    lua.set_function("md5", [](std::string_view s) {
        uint8_t buf[md5::DIGEST_BYTES] = { 0 };
        md5::md5_context ctx;
        md5::init(ctx);
        md5::update(ctx, s.data(), s.size());
        md5::finish(ctx, buf);

        std::string res(md5::DIGEST_BYTES * 2, '\0');
        for (size_t i = 0; i < 16; i++) {
            int t = buf[i];
            int a = t / 16;
            int b = t % 16;
            res[i * 2] = md5::HEX[a];
            res[i * 2 + 1] = md5::HEX[b];
        }
        return res;
        });

    lua.set_function("tostring", [](lua_State* L) {
        const char* data = (const char*)lua_touserdata(L, 1);
        size_t len = luaL_checkinteger(L, 2);
        lua_pushlstring(L, data, len);
        return 1;
        });

    lua.set_function("localtime", [](lua_State* L) {
        time_t t = luaL_checkinteger(L, 1);
        std::tm local_tm;
        time::localtime(&t, &local_tm);
        lua_createtable(L, 0, 8);
        luaL_rawsetfield(L, -3, "yeay", lua_pushinteger(L, (lua_Integer)local_tm.tm_year + 1900));
        luaL_rawsetfield(L, -3, "month", lua_pushinteger(L, (lua_Integer)local_tm.tm_mon + 1));
        luaL_rawsetfield(L, -3, "day", lua_pushinteger(L, local_tm.tm_mday));
        luaL_rawsetfield(L, -3, "hour", lua_pushinteger(L, local_tm.tm_hour));
        luaL_rawsetfield(L, -3, "min", lua_pushinteger(L, local_tm.tm_min));
        luaL_rawsetfield(L, -3, "sec", lua_pushinteger(L, local_tm.tm_sec));
        luaL_rawsetfield(L, -3, "weekday", lua_pushinteger(L, local_tm.tm_wday));
        luaL_rawsetfield(L, -3, "yearday", lua_pushinteger(L, local_tm.tm_yday));
        return 1;
        });

    lua.set_function("dailytime", [](lua_State* L) {
        time_t t = luaL_checkinteger(L, 1);
        std::tm local_tm;
        time::localtime(&t, &local_tm);
        auto t2 = time::make_time(local_tm.tm_year + 1900, local_tm.tm_mon + 1, local_tm.tm_mday, 0, 0, 0);
        lua_pushinteger(L, t2);
        return 1;
        });

    lua.set("timezone", time::timezone());

    lua_extend_library(lua.lua_state(), lua_table_new, "table", "new");
    lua_extend_library(lua.lua_state(), lua_string_hash, "string", "hash");
    lua_extend_library(lua.lua_state(), lua_string_hex, "string", "hex");

    return *this;
}

const lua_bind& lua_bind::bind_log(lua_service* service) const
{
    lua.set_function("set_loglevel", (void(moon::log::*)(std::string_view)) & log::set_level, service->logger());
    lua.set_function("get_loglevel", &log::get_level, service->logger());

    sol_extend_library(lua, console_log, "error", [service](lua_State* L)->int {
        lua_pushlightuserdata(L, service);
        lua_pushinteger(L, (int)moon::LogLevel::Error);
        return 2;
        });

    sol_extend_library(lua, console_log, "warn", [service](lua_State* L)->int {
        lua_pushlightuserdata(L, service);
        lua_pushinteger(L, (int)moon::LogLevel::Warn);
        return 2;
        });

    sol_extend_library(lua, console_log, "info", [service](lua_State* L)->int {
        lua_pushlightuserdata(L, service);
        lua_pushinteger(L, (int)moon::LogLevel::Info);
        return 2;
        });

    sol_extend_library(lua, console_log, "debug", [service](lua_State* L)->int {
        lua_pushlightuserdata(L, service);
        lua_pushinteger(L, (int)moon::LogLevel::Debug);
        return 2;
        });
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
    lua.set_function("cpu", &lua_service::cpu_cost, s);
    lua.set_function("make_prefab", &worker::make_prefab, worker_);
    lua.set_function("send_prefab", [worker_, s](uint32_t receiver, uint32_t cacheid, const std::string_view& header, int32_t sessionid, uint8_t type) {
        worker_->send_prefab(s->id(), receiver, cacheid, header, sessionid, type);
        });
    lua.set_function("send", &router::send, router_);
    lua.set_function("new_service", &router::new_service, router_);
    lua.set_function("remove_service", &router::remove_service, router_);
    lua.set_function("runcmd", &router::runcmd, router_);
    lua.set_function("broadcast", &router::broadcast, router_);
    lua.set_function("queryservice", &router::get_unique_service, router_);
    lua.set_function("set_env", &router::set_env, router_);
    lua.set_function("get_env", &router::get_env, router_);
    lua.set_function("wstate", &router::worker_info, router_);
    lua.set_function("exit", &server::stop, server_);
    lua.set_function("service_count", &server::service_count, server_);
    lua.set_function("now", [server_]() {return server_->now(); });
    lua.set_function("adjtime", [server_](int64_t v) {
        bool ok = time::offset(v);
        server_->now(true);
        return ok;
        });
    return *this;
}

const lua_bind& lua_bind::bind_socket(lua_service* s) const
{
    auto w = s->get_worker();
    auto& sock = w->socket();

    sol::table tb = lua.create();

    tb.set_function("try_open", &moon::socket::try_open, &sock);
    tb.set_function("listen", [&sock, s](const std::string& host, uint16_t port, uint8_t type) {
        return sock.listen(host, port, s->id(), type);
        });

    tb.set_function("accept", &moon::socket::accept, &sock);
    tb.set_function("connect", &moon::socket::connect, &sock);
    tb.set_function("read", &moon::socket::read, &sock);
    tb.set_function("write", &moon::socket::write, &sock);
    tb.set_function("write_with_flag", &moon::socket::write_with_flag, &sock);
    tb.set_function("write_message", &moon::socket::write_message, &sock);
    tb.set_function("close", &moon::socket::close, &sock);
    tb.set_function("settimeout", &moon::socket::settimeout, &sock);
    tb.set_function("setnodelay", &moon::socket::setnodelay, &sock);
    tb.set_function("set_enable_chunked", &moon::socket::set_enable_chunked, &sock);
    tb.set_function("set_send_queue_limit", &moon::socket::set_send_queue_limit, &sock);
    tb.set_function("getaddress", &moon::socket::getaddress, &sock);

	

    registerlib(lua.lua_state(), "asio", tb);
    return *this;
}

void lua_bind::registerlib(lua_State* L, const char* name, lua_CFunction f)
{
    luaL_requiref(L, name, f, 0);
    lua_pop(L, 1); /* pop result*/
    assert(lua_gettop(L) == 0);
}

void lua_bind::registerlib(lua_State* L, const char* name, const sol::table& module)
{
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded"); /* get 'package.loaded' */
    module.push();
    lua_setfield(L, -2, name); /* package.loaded[name] = f */
    lua_pop(L, 2); /* pop 'package' and 'loaded' tables */
    assert(lua_gettop(L) == 0);
}
