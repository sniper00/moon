#include "lua_bind.h"
#include "common/time.hpp"
#include "common/timer.hpp"
#include "common/path.hpp"
#include "common/buffer_writer.hpp"
#include "common/hash.hpp"
#include "common/http_request.hpp"
#include "components/tcp/tcp.h"

#include "message.hpp"
#include "server.h"
#include "log.h"
#include "lua_buffer.hpp"
#include "lua_string_view.hpp"
#include "lua_serialize.hpp"

#include "services/lua_service.h"


 //WRAP_FUNCTION : http://sol2.readthedocs.io/en/latest/performance.html
#define WRAP_FUNCTION(f) sol::c_call<sol::wrap<decltype(f),(f)>>

using namespace moon;

lua_bind::lua_bind(sol::table& l)
    :lua(l)
{
}

lua_bind::~lua_bind()
{
}

const lua_bind & lua_bind::bind_timer(moon::timer* t) const
{
    lua.set_function("set_on_timer", &moon::timer::set_on_timer, t);
    lua.set_function("set_remove_timer", &moon::timer::set_remove_timer, t);
    lua.set_function("repeated", &moon::timer::repeat, t);
    lua.set_function("remove_timer", &moon::timer::remove, t);
    lua.set_function("pause_timer", &moon::timer::stop_all_timer, t);
    lua.set_function("start_all_timer", &moon::timer::start_all_timer, t);
    return *this;
}

int new_table(lua_State* L)
{
    auto arrn = luaL_checkinteger(L, -2);
    auto hn = luaL_checkinteger(L, -1);
    lua_createtable(L, (int)arrn, (int)hn);
    return 1;
}

const lua_bind & lua_bind::bind_util() const
{
    lua.set_function("millsecond", WRAP_FUNCTION(&time::millsecond));
    lua.set_function("sleep", [](int64_t ms) { thread_sleep(ms); });
    lua.set_function("hash_string", [](const std::string& s) { return moon::hash_range(s.begin(), s.end()); });

    lua_getglobal(lua.lua_state(), "table");
    lua_pushcclosure(lua.lua_state(), new_table, 0);
    lua_setfield(lua.lua_state(), -2, "new_table");
    return *this;
}

const lua_bind & lua_bind::bind_path() const
{
    lua.set_function("traverse_folder", path::traverse_folder);
    lua.set_function("exist", path::exist);
    lua.set_function("create_directory", path::create_directory);
    lua.set_function("directory", path::directory);
    lua.set_function("extension", path::extension);
    lua.set_function("filename", path::filename);
    lua.set_function("name_without_extension", path::name_without_extension);
    lua.set_function("current_directory", path::current_directory);
    return *this;
}

const lua_bind & lua_bind::bind_log(moon::log* logger) const
{
    lua.set_function("LOGV",&moon::log::logstring,logger);
    return *this;
}

static void redirect_message(message* m, const moon::string_view_t& header, uint32_t receiver)
{
    if (header.size() != 0)
    {
        m->set_header(header);
    }
    m->set_receiver(receiver);
}

const lua_bind & lua_bind::bind_message() const
{
    lua.new_usertype<message>("message"
        , sol::call_constructor, sol::no_constructor
        , "sender", WRAP_FUNCTION(&message::sender)
        , "responseid", WRAP_FUNCTION(&message::responseid)
        , "receiver", WRAP_FUNCTION(&message::receiver)
        , "type", WRAP_FUNCTION(&message::type)
        , "subtype", WRAP_FUNCTION(&message::subtype)
        , "header", WRAP_FUNCTION(&message::header)
        , "bytes", WRAP_FUNCTION(&message::bytes)
        , "size", WRAP_FUNCTION(&message::size)
        , "subbytes", WRAP_FUNCTION(&message::subbytes)
        , "buffer", WRAP_FUNCTION(&message::pointer)
        , "redirect", (redirect_message)
        );
    return *this;
}

const lua_bind& lua_bind::bind_service(lua_service* s) const
{
    auto server_ = s->get_server();

    lua.set("null", (void*)(server_));

    auto broadcast = [server_](uint32_t sender, const buffer_ptr_t & data, uint8_t type)
    {
        auto msg = message::create(data);
        msg->set_type(type);
        server_->broadcast(sender, msg);
    };

    lua.set_function("name", &lua_service::name, s);
    lua.set_function("id", &lua_service::id, s);
    lua.set_function("send_cache", &lua_service::send_cache, s);
    lua.set_function("make_cache", &lua_service::make_cache, s);
    lua.set_function("add_component_tcp", &lua_service::add_component_tcp, s);
    lua.set_function("get_component_tcp", &lua_service::get_component_tcp, s);
    lua.set_function("remove_component", &lua_service::remove, s);
    lua.set_function("set_init", &lua_service::set_init,s);
    lua.set_function("set_start", &lua_service::set_start,s);
    lua.set_function("set_exit", &lua_service::set_exit,s);
    lua.set_function("set_dispatch", &lua_service::set_dispatch,s);
    lua.set_function("set_destroy", &lua_service::set_destroy,s);
    lua.set_function("memory_use", &lua_service::memory_use, s);
    lua.set_function("send", &server::send, server_);
    lua.set_function("new_service", &server::new_service, server_);
    lua.set_function("runcmd", &server::runcmd, server_);
    lua.set_function("broadcast", broadcast);
    lua.set_function("workernum", &server::workernum, server_);
    lua.set_function("local_db", &server::local_db, server_);
    lua.set_function("unique_service", &server::get_unique_service, server_);
    lua.set_function("set_unique_service", &server::set_unique_service, server_);
    lua.set_function("stop", &server::stop, server_);
    lua.set_function("set_env", &server::set_env, server_);
    lua.set_function("get_env", &server::get_env, server_);
    lua.set_function("set_loglevel", [server_](string_view_t s) { server_->logger()->set_level(s); });
    return *this;
}

const lua_bind & lua_bind::bind_socket() const
{
    lua.new_usertype<moon::tcp>("tcp"
        , sol::call_constructor, sol::no_constructor
        , "async_accept", WRAP_FUNCTION(&moon::tcp::async_accept)
        , "connect", WRAP_FUNCTION(&moon::tcp::connect)
        , "async_connect", WRAP_FUNCTION(&moon::tcp::async_connect)
        , "listen", WRAP_FUNCTION(&moon::tcp::listen)
        , "close", WRAP_FUNCTION(&moon::tcp::close)
        , "read", WRAP_FUNCTION(&moon::tcp::read)
        , "send", WRAP_FUNCTION(&moon::tcp::send)
        , "send_message", WRAP_FUNCTION(&moon::tcp::send_message)
        , "setprotocol", WRAP_FUNCTION(&moon::tcp::setprotocol)
        , "settimeout", WRAP_FUNCTION(&moon::tcp::settimeout)
        , "setnodelay", WRAP_FUNCTION(&moon::tcp::setnodelay)
        );
    return *this;
}

const lua_bind & lua_bind::bind_http() const
{
    string_view_t method;
    string_view_t path;
    string_view_t query_string;
    string_view_t http_version;
    string_view_t remote_endpoint_address;
    lua.new_usertype<moon::http_request>("http_request"
        , sol::constructors<sol::types<>>()
        , "parse", WRAP_FUNCTION(&moon::http_request::parse_string)
        , "method",sol::readonly(&moon::http_request::method)
        , "path", sol::readonly(&moon::http_request::path)
        , "query_string", sol::readonly(&moon::http_request::query_string)
        , "http_version", sol::readonly(&moon::http_request::http_version)
        );

    return *this;
}

const char* lua_traceback(lua_State * L)
{
    luaL_traceback(L, L, NULL, 1);
    auto s =  lua_tostring(L, -1);
    if (nullptr != s)
    {
        return "";
    }
    return s;
}
