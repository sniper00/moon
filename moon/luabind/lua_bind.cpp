#include "lua_bind.h"
#include "common/time.hpp"
#include "common/timer.hpp"
#include "common/path.hpp"
#include "common/buffer_writer.hpp"
#include "common/hash.hpp"
#include "common/http_request.hpp"
#include "common/log.hpp"
#include "components/tcp/tcp.h"
#include "message.hpp"
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

const lua_bind & lua_bind::bind_timer(moon::timer_t* t) const
{
    lua.set_function("repeated", &moon::timer_t::repeat, t);
    lua.set_function("remove_timer", &moon::timer_t::remove, t);
    lua.set_function("pause_timer", &moon::timer_t::stop_all_timer, t);
    lua.set_function("start_all_timer", &moon::timer_t::start_all_timer, t);
    return *this;
}

int new_table(lua_State* L)
{
    auto arrn = luaL_checkinteger(L, -2);
    auto hn = luaL_checkinteger(L, -1);
    lua_createtable(L, (int)arrn, (int)hn);
    return 1;
}

std::string make_cluster_message(string_view_t header, string_view_t data)
{
    std::string ret;
    uint16_t len = static_cast<uint16_t>(data.size());
    ret.append((const char*)&len, sizeof(len));
    ret.append(data.data(), data.size());
    ret.append(header.data(), header.size());
    return ret;
}

void pack_cluster_message(string_view_t header, message* msg)
{
    uint16_t len = static_cast<uint16_t>(msg->size());
    msg->get_buffer()->write_front(&len, 0, 1);
    msg->get_buffer()->write_back(header.data(), 0, header.size());
}

string_view_t unpack_cluster_message(message* msg)
{
    uint16_t len = 0;
    msg->get_buffer()->read(&len, 0, 1);
    size_t header_size = msg->size() - len;
    const char* header = msg->data() + len;
    int tmp = (int)header_size;
    msg->get_buffer()->offset_writepos(-tmp);
    return string_view_t{ header,header_size };
}

const lua_bind & lua_bind::bind_util() const
{
    lua.set_function("millsecond", (&time::millsecond));
    lua.set_function("sleep", [](int64_t ms) { thread_sleep(ms); });
    lua.set_function("hash_string", [](const std::string& s) { return moon::hash_range(s.begin(), s.end()); });
    lua.set_function("hex_string", (&moon::hex_string));

    lua.set_function("pack_cluster", pack_cluster_message);
    lua.set_function("unpack_cluster", unpack_cluster_message);
    lua.set_function("make_cluster_message", make_cluster_message);

    lua_getglobal(lua.lua_state(), "table");
    lua_pushcclosure(lua.lua_state(), new_table, 0);
    lua_setfield(lua.lua_state(), -2, "new_table");
    return *this;
}

static void traverse_folder(std::string path,int nDepth,sol::protected_function func,sol::this_state s)
{
	path::traverse_folder(path, nDepth, [func,s](const std::string& filepath, int nDepth)->bool {
		auto result = func(filepath, nDepth);
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

const lua_bind & lua_bind::bind_path() const
{
    lua.set_function("traverse_folder", traverse_folder);
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
        , "buffer", (&message::pointer)
        , "redirect", (redirect_message)
        , "resend", resend
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
    lua.set_function("add_tcp", &lua_service::add_tcp, s);
    lua.set_function("get_tcp", &lua_service::get_tcp, s);
    lua.set_function("remove_component", &lua_service::remove, s);
    lua.set_function("set_init", &lua_service::set_init,s);
    lua.set_function("set_start", &lua_service::set_start,s);
    lua.set_function("set_exit", &lua_service::set_exit,s);
    lua.set_function("set_dispatch", &lua_service::set_dispatch,s);
    lua.set_function("set_destroy", &lua_service::set_destroy,s);
	lua.set_function("set_on_timer", &lua_service::set_on_timer, s);
	lua.set_function("set_remove_timer", &lua_service::set_remove_timer, s);
    lua.set_function("memory_use", &lua_service::memory_use, s);
    lua.set_function("send", &server::send, server_);
    lua.set_function("new_service", &server::new_service, server_);
    lua.set_function("runcmd", &server::runcmd, server_);
    lua.set_function("broadcast", broadcast);
    lua.set_function("workernum", &server::workernum, server_);
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
        , "async_accept", (&moon::tcp::async_accept)
        , "connect", (&moon::tcp::connect)
        , "async_connect", (&moon::tcp::async_connect)
        , "listen", (&moon::tcp::listen)
        , "close", (&moon::tcp::close)
        , "read", (&moon::tcp::read)
        , "send", (&moon::tcp::send)
        , "send_then_close", (&moon::tcp::send_then_close)
        , "send_message", (&moon::tcp::send_message)
        , "setprotocol", (&moon::tcp::setprotocol)
        , "settimeout", (&moon::tcp::settimeout)
        , "setnodelay", (&moon::tcp::setnodelay)
        , "set_enable_frame", (&moon::tcp::set_enable_frame)
        );
    return *this;
}

const lua_bind & lua_bind::bind_http() const
{
    lua.new_usertype<moon::http_request>("http_request"
        , sol::constructors<sol::types<>>()
        , "parse", (&moon::http_request::parse_string)
        , "method",sol::readonly(&moon::http_request::method)
        , "path", sol::readonly(&moon::http_request::path)
        , "query_string", sol::readonly(&moon::http_request::query_string)
        , "http_version", sol::readonly(&moon::http_request::http_version)
        , "get_header", (&moon::http_request::get_header)
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
