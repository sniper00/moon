#include "lua_bind.h"
#include "common/time.hpp"
#include "common/timer.hpp"
#include "common/hash.hpp"
#include "common/log.hpp"
#include "common/sha1.hpp"
#include "common/md5.hpp"
#include "message.hpp"
#include "server.h"
#include "worker.h"
#include "lua_buffer.hpp"
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

int lua_serialize_do_unpack(lua_State* L, const char* data, size_t len);

static int unpack_cluster_header(lua_State* L)
{
    auto buf = reinterpret_cast<buffer*>(lua_touserdata(L, 1));
    uint16_t dlen = 0;
    buf->read(&dlen, 1);
    int hlen = static_cast<int>(buf->size() - dlen);
    buf->seek(dlen);
    auto n = lua_serialize_do_unpack(L,buf->data(),buf->size());
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
        if (i > 1) buf.write_back("\t",1);
        buf.write_back(s, l);
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

    lua.set_function("md5", [](std::string_view s) {
        uint8_t buf[md5::DIGEST_BYTES] = { 0 };
        md5::md5_context ctx;
        md5::init(ctx);
        md5::update(ctx, s.data(), s.size());
        md5::finish(ctx, buf);

        std::string res(md5::DIGEST_BYTES*2,'\0');
        for (size_t i = 0; i < 16; i++) {
            int t = buf[i];
            int a = t / 16;
            int b = t % 16;
            res.append(1, md5::HEX[a]);
            res.append(1, md5::HEX[b]);
        }
        return res;
    });

    sol_extend_library(lua, unpack_cluster_header, "unpack_cluster_header");

    lua_extend_library(lua.lua_state(), lua_table_new, "table", "new");
    lua_extend_library(lua.lua_state(), lua_string_hash, "string", "hash");
    lua_extend_library(lua.lua_state(), lua_string_hex, "string", "hex");

    lua.new_enum<moon::buffer_flag>("buffer_flag", {
        {"close",moon::buffer_flag::close},
        {"ws_text",moon::buffer_flag::ws_text},
        {"ws_ping",moon::buffer_flag::ws_ping},
        {"ws_pong",moon::buffer_flag::ws_pong}
    });

    lua.new_enum<moon::buffer::seek_origin>("seek_origin", {
        {"begin",moon::buffer::seek_origin::Begin},
        {"current",moon::buffer::seek_origin::Current},
        {"end",moon::buffer::seek_origin::End}
    });
    return *this;
}

const lua_bind & lua_bind::bind_log(moon::log* logger, uint32_t serviceid) const
{
    lua.set_function("LOGV", &moon::log::logstring, logger);
    register_lua_print(lua, logger, serviceid);
    return *this;
}

const lua_bind & lua_bind::bind_message() const
{
    //https://sol2.readthedocs.io/en/latest/functions.html?highlight=lua_CFunction
    //Note that stateless lambdas can be converted to a function pointer, 
    //so stateless lambdas similar to the form [](lua_State*) -> int { ... } will also be pushed as raw functions.
    //If you need to get the Lua state that is calling a function, use sol::this_state.

    auto write_front = [](lua_State* L)->int
    {
        auto m = sol::stack::get<message*>(L, 1);
        size_t len = 0;
        auto data = luaL_checklstring(L, 2, &len);
        m->get_buffer()->write_front(data, len);
        return 0;
    };

    auto write_back = [](lua_State* L)->int
    {
        auto m = sol::stack::get<message*>(L, 1);
        size_t len = 0;
        auto data = luaL_checklstring(L, 2, &len);
        m->get_buffer()->write_back(data, len);
        return 0;
    };

    auto seek = [](lua_State* L)->int
    {
        auto m = sol::stack::get<message*>(L, 1);
        auto pos = static_cast<int>(luaL_checkinteger(L, 2));
        auto origin = static_cast<buffer::seek_origin>(luaL_checkinteger(L, 3));
        m->get_buffer()->seek(pos, origin);
        return 0;
    };

    auto offset_writepos = [](lua_State* L)->int
    {
        auto m = sol::stack::get<message*>(L, 1);
        auto offset = static_cast<int>(luaL_checkinteger(L, 2));
        m->get_buffer()->offset_writepos(offset);
        return 0;
    };

    auto cstring = [](lua_State* L)->int
    {
        auto m = sol::stack::get<message*>(L, -1);
        lua_pushlightuserdata(L, (void*)(m->data()));
        lua_pushinteger(L, m->size());
        return 2;
    };

    auto tobuffer = [](lua_State* L)->int
    {
        auto m = sol::stack::get<message*>(L, -1);
        lua_pushlightuserdata(L, (void*)m->get_buffer());
        return 1;
    };

    auto redirect = [](lua_State* L)->int
    {
        auto m = sol::stack::get<message*>(L, 1);
        size_t hlen = 0;
        auto hdata = luaL_checklstring(L, 2, &hlen);
        auto receiver = static_cast<uint32_t>(luaL_checkinteger(L, 3));
        auto type = static_cast<uint8_t>(luaL_checkinteger(L, 4));

        if (hlen != 0)
        {
            m->set_header(std::string_view{ hdata, hlen });
        }
        m->set_receiver(receiver);
        m->set_type(type);
        return 0;
    };

    auto resend = [](lua_State* L)->int
    {
        auto m = sol::stack::get<message*>(L, 1);
        auto sender = static_cast<uint32_t>(luaL_checkinteger(L, 2));
        auto receiver = static_cast<uint32_t>(luaL_checkinteger(L, 3));
        size_t hlen = 0;
        auto hdata = luaL_checklstring(L, 4, &hlen);
        auto sessionid = static_cast<int32_t>(luaL_checkinteger(L, 5));
        auto type = static_cast<uint8_t>(luaL_checkinteger(L, 6));

        if (hlen != 0)
        {
            m->set_header(std::string_view{ hdata, hlen });
        }
        m->set_sender(sender);
        m->set_receiver(receiver);
        m->set_type(type);
        m->set_sessionid(-sessionid);
        return 0;
    };

    lua.new_usertype<message>("message"
        , sol::call_constructor, sol::no_constructor
        , "sender", (&message::sender)
        , "sessionid", (&message::sessionid)
        , "subtype", (&message::subtype)
        , "header", (&message::header)
        , "bytes", (&message::bytes)
        , "size", (&message::size)
        , "substr", (&message::substr)
        , "buffer", tobuffer
        , "redirect", redirect
        , "resend", resend
        , "cstring", cstring
        , "write_front", write_front
        , "write_back", write_back
        , "seek", seek
        , "offset_writepos", offset_writepos
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
    tb.set_function("connect", [&sock, s](const std::string& host, uint16_t port, int32_t sessionid, uint8_t type, int32_t timeout) {
        return sock.connect(host, port, s->id(), type, sessionid, timeout);
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
    tb.set_function("set_send_queue_limit", &moon::socket::set_send_queue_limit, &sock);
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

