#include "LuaBind.h"
#include "common/time.hpp"
#include "common/path.hpp"
#include "common/buffer_writer.hpp"
#include "log.h"
#include "timer.h"
#include "message.h"
#include "service_pool.h"
#include "LuaService.h"
#include "sol.hpp"
#include "http_client.h"

using namespace moon;

LuaBind::LuaBind(sol::state& l)
	:lua(l)
{
}


LuaBind::~LuaBind()
{
}

LuaBind& LuaBind::BindTime()
{
	sol::table tb = lua.create_named_table("Time");
	tb.set_function("GetSecond", time::second);
	tb.set_function("GetMillsecond", time::millsecond);
	tb.set_function("MakeTime", time::make_time);
	return *this;
}

LuaBind & LuaBind::BindTimer()
{
	lua.new_usertype<moon::timer>("MoonTimer"
		, sol::call_constructor, sol::no_constructor
		, "ExpiredOnce", &moon::timer::expired_once
		, "Repeat", &moon::timer::repeat
		, "Remove", &moon::timer::remove
		, "StopAllTimer", &moon::timer::stop_all_timer
		, "StartAllTimer", &moon::timer::start_all_timer
		);

	return *this;
}

LuaBind & LuaBind::BindUtil()
{
	sol::table tb = lua.create_named_table("Util");
	tb.set_function("HashString", [](const std::string& s) { return std::hash<std::string>()(s);});
	tb.set_function("Sleep", [](int64_t ms) { thread_sleep(ms); });
	return *this;
}

LuaBind & LuaBind::BindPath()
{
	sol::table tb = lua.create_named_table("Path");
	tb.set_function("TraverseFolder", path::traverse_folder);
	tb.set_function("Exist", path::exist);
	tb.set_function("CreateDirectory", path::create_directory);
	tb.set_function("GetDirectory", path::directory);
	tb.set_function("GetExtension", path::extension);
	tb.set_function("GetName", path::filename);
	tb.set_function("GetNameWithoutExtension", path::name_without_extension);
	tb.set_function("GetCurrentDir", path::current_directory);
	return *this;
}

LuaBind & LuaBind::BindLog()
{
	lua.new_enum("LogLevel"
		, "Error", LogLevel::Error
		, "Warn", LogLevel::Warn
		, "Info", LogLevel::Info
		, "Debug", LogLevel::Debug
		, "Trace", LogLevel::Trace
	);

	lua.set_function("LOGV", [](bool bconsole, LogLevel lv, const std::string& tag, const std::string& content) {
		log::logstring(bconsole, lv, tag.data(), content.data());
	});
	return *this;
}

LuaBind& LuaBind::BindMessageType()
{
	lua.new_enum("message_type"
		, "unknown", message_type::unknown
		, "service_send", message_type::service_send
		, "service_response", message_type::service_response
		, "service_client", message_type::service_client
		, "network_connect", message_type::network_connect
		, "network_recv", message_type::network_recv
		, "network_close", message_type::network_close
		, "network_error", message_type::network_error
		, "network_logic_error", message_type::network_logic_error
		, "system_service_crash", message_type::system_service_crash
		, "system_network_report", message_type::system_network_report
	);

	return *this;
}

#define WRAP_FUNCTION(f) sol::c_call<sol::wrap<decltype(f),f>>

LuaBind & LuaBind::BindMessage()
{
	lua.new_usertype<message>("message"
		, sol::call_constructor, sol::no_constructor
		, "sender", WRAP_FUNCTION(&message::sender)
		, "rpc", WRAP_FUNCTION(&message::rpc)
		, "receiver", WRAP_FUNCTION(&message::receiver)
		, "type", WRAP_FUNCTION(&message::type)
		, "userctx", WRAP_FUNCTION(&message::userctx)
		, "bytes", WRAP_FUNCTION(&message::bytes)
		);
	return *this;
}

LuaBind& LuaBind::BindService()
{
	auto net_send = []
	(LuaService* s, uint32_t sessionid,const std::string& data)
	{
		auto buf = create_buffer(data.size(), sizeof(message_head));
		buffer_writer<buffer> bw(*buf);
		bw.write_array(data.data(), data.size());
		s->net_send(sessionid, buf);
	};

	lua.new_usertype<LuaService>("LuaService"
		, sol::call_constructor, sol::no_constructor
		, "name", WRAP_FUNCTION(&LuaService::name)
		, "serviceid", WRAP_FUNCTION(&LuaService::serviceid)
		, "set_enbale_update", WRAP_FUNCTION(&LuaService::set_enbale_update)
		, "enable_update", WRAP_FUNCTION(&LuaService::enable_update)
		, "send", WRAP_FUNCTION(&LuaService::send)
		, "send_cache", WRAP_FUNCTION(&LuaService::send_cache)
		, "broadcast", WRAP_FUNCTION(&LuaService::broadcast)
		, "make_cache", WRAP_FUNCTION(&LuaService::make_cache)
		, "new_service", WRAP_FUNCTION(&LuaService::new_service)
		, "remove_service", WRAP_FUNCTION(&LuaService::remove_service)
		, "init_net", WRAP_FUNCTION(&LuaService::init_net)
		, "listen", WRAP_FUNCTION(&LuaService::listen)
		, "async_connect", WRAP_FUNCTION(&LuaService::async_connect)
		, "sync_connect", WRAP_FUNCTION(&LuaService::sync_connect)
		, "net_send",net_send
		, "get_config", WRAP_FUNCTION(&LuaService::get_config)
		);
	return *this;
}

LuaBind& LuaBind::BindServicePool()
{
	auto new_service = [](service_pool* pool, const std::string& config, bool exclusived)
	{
		auto s = std::make_shared<LuaService>();
		pool->new_service(s, config, exclusived);
	};

	auto new_native_service = [](service_pool* pool,const std::shared_ptr<service>& s,const std::string& config, bool exclusived)
	{
		pool->new_service(s, config, exclusived);
	};

	lua.new_usertype<service_pool>("service_pool"
		, sol::call_constructor, sol::no_constructor
		, "init", &service_pool::init
		, "machineid", &service_pool::machineid
		, "new_service",new_service
		, "new_native_service", new_native_service
		, "remove_service", &service_pool::remove_service
		, "broadcast", &service_pool::broadcast
		, "run", &service_pool::run
		, "stop", &service_pool::stop
		);

	return *this;
}

LuaBind & LuaBind::BindHttp()
{
	lua.new_usertype<http_request>("http_request"
		, sol::call_constructor, sol::no_constructor
		, "set_request_type", &http_request::set_request_type
		, "set_path", &http_request::set_path
		, "set_content", &http_request::set_content
		, "push_header", &http_request::push_header
		);

	lua.new_usertype<http_client>("http_client"
		, sol::constructors<sol::types<const std::string&>>()
		, "make_request", &http_client::make_request
		, "request", &http_client::request
		, "get_content", &http_client::get_content
		);

	return *this;
}

std::string Traceback(lua_State * _state)
{
	lua_Debug info;
	int level = 0;
	std::string outputTraceback;
	char buffer[4096];

	while (lua_getstack(_state, level, &info)) {
		lua_getinfo(_state, "nSl", &info);

#if defined(_WIN32) && defined(_MSC_VER)
		sprintf_s(buffer, "  [%d] %s:%d -- %s [%s]\n",
			level, info.short_src, info.currentline,
			(info.name ? info.name : "<unknown>"), info.what);
#else
		sprintf(buffer, "  [%d] %s:%d -- %s [%s]\n",
			level, info.short_src, info.currentline,
			(info.name ? info.name : "<unknown>"), info.what);
#endif
		outputTraceback.append(buffer);
		++level;
	}
	return outputTraceback;
}
