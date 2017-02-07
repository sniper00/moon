#include "LuaBind.h"
#include "common/time.hpp"
#include "common/path.hpp"
#include "common/buffer_writer.hpp"
#include "common/hash.hpp"
#include "log.h"
#include "message.h"
#include "service_pool.h"
#include "components/timer/timer.h"
#include "components/http/http_client.h"
#include "components/tcp/network.h"

#include "LuaService.h"
#include "sol.hpp"

#include "crypto/sha1.h"

/**
	可以使用这个宏绑定 函数 see http://sol2.readthedocs.io/en/latest/performance.html
	稍微提高调用性能，但不能用于继承自父类的函数，会造成类型检查不通过
**/
#define WRAP_FUNCTION(f) sol::c_call<sol::wrap<decltype(f),(f)>>

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
		, "ExpiredOnce", WRAP_FUNCTION(&moon::timer::expired_once)
		, "Repeat", WRAP_FUNCTION(&moon::timer::repeat)
		, "Remove", WRAP_FUNCTION(&moon::timer::remove)
		, "StopAllTimer", WRAP_FUNCTION(&moon::timer::stop_all_timer)
		, "StartAllTimer", WRAP_FUNCTION(&moon::timer::start_all_timer)
		);

	return *this;
}

std::string sha1_hash(const std::string& s)
{
	uint8_t hash_[20] = { 0 };
	SHA1_CTX ctx;
	SHA1Init(&ctx);
	SHA1Update(&ctx, (uint8_t*)s.data(), (uint32_t)s.size());
	SHA1Final(hash_, &ctx);

	std::stringstream ss;
	ss << std::hex;
	for (int i = 0; i < 20; i++)
	{
		ss << (uint32_t)hash_[i];
	}
	return ss.str();
};

LuaBind & LuaBind::BindUtil()
{
	sol::table tb = lua.create_named_table("Util");
	tb.set_function("HashString", [](const std::string& s) { return moon::hash_range(s.begin(), s.end());});
	tb.set_function("Sleep", [](int64_t ms) { thread_sleep(ms); });
	tb.set_function("SHA1", sha1_hash);
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

	lua.set_function("LOGV", log::logstring);
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
		, "message_system", message_type::message_system
	);
	return *this;
}

LuaBind & LuaBind::BindMessage()
{
	lua.new_usertype<message>("message"
		, sol::call_constructor, sol::no_constructor
		, "sender", WRAP_FUNCTION(&message::sender)
		, "rpc", WRAP_FUNCTION(&message::rpc)
		, "receiver", WRAP_FUNCTION(&message::receiver)
		, "set_receiver", WRAP_FUNCTION(&message::set_receiver)
		, "type", WRAP_FUNCTION(&message::type)
		, "userctx", WRAP_FUNCTION(&message::userctx)
		, "set_userctx", WRAP_FUNCTION(&message::set_userctx)
		, "bytes", WRAP_FUNCTION(&message::bytes)
		, "check_uint16", WRAP_FUNCTION(&message::check_uint16)
		);
	return *this;
}

LuaBind& LuaBind::BindService()
{
	auto getnetwork = [](service* s)->network*
	{
		auto n = s->get_component<network>("network");
		if (n != nullptr)
		{
			return n.get();
		}
		return nullptr;
	};

	auto broadcast = [](service* s,const std::string & data, message_type type)
	{
		auto msg = message::create(data.size());
		msg->write_data(data);
		msg->set_type(type);
		s->broadcast(msg);
	};

	auto register_service = [](const std::string& type)
	{
		service_pool::instance()->register_service(type, []()->service_ptr_t {
			return std::make_shared<LuaService>();
		});
	};

	lua.new_usertype<service>("lua_service"
		, sol::call_constructor, sol::no_constructor
		, "name", (&service::name)
		, "serviceid", WRAP_FUNCTION(&service::serviceid)
		, "set_enable_update", (&service::set_enable_update)
		, "enable_update", (&service::enable_update)
		, "send", WRAP_FUNCTION(&service::send)
		, "send_cache", WRAP_FUNCTION(&service::send_cache)
		, "make_cache", WRAP_FUNCTION(&service::make_cache)
		, "new_service", WRAP_FUNCTION(&service::new_service)
		, "remove_service", WRAP_FUNCTION(&service::remove_service)
		, "report_service_num", WRAP_FUNCTION(&service::report_service_num)
		, "report_work_time", WRAP_FUNCTION(&service::report_work_time)
		, "broadcast", broadcast
		, "get_network", getnetwork
		, "register", register_service
		);
	return *this;
}

LuaBind& LuaBind::BindServicePool()
{
	auto broadcast = [](service_pool* pool, uint32_t sender, const std::string & data, message_type type)
	{
		auto msg = message::create(data.size());
		msg->write_data(data);
		msg->set_type(type);
		pool->broadcast(sender, msg);
	};

	lua.new_usertype<service_pool>("service_pool"
		, sol::call_constructor, sol::no_constructor
		, "init", WRAP_FUNCTION(&service_pool::init)
		, "machineid", WRAP_FUNCTION(&service_pool::machineid)
		, "new_service", WRAP_FUNCTION(&service_pool::new_service)
		, "remove_service", WRAP_FUNCTION(&service_pool::remove_service)
		, "broadcast", broadcast
		, "run", WRAP_FUNCTION(&service_pool::run)
		, "stop", WRAP_FUNCTION(&service_pool::stop)
		, "report_service_num", WRAP_FUNCTION(&service_pool::report_service_num)
		, "report_work_time", WRAP_FUNCTION(&service_pool::report_work_time)
		);

	return *this;
}

void network_send(network* n, uint32_t sessionid, const std::string& data)
{
	auto buf = create_buffer(data.size(), sizeof(message_size_t));
	buf->write_back(data.data(), 0, data.size());
	n->send(sessionid, buf);
}

void network_send_message(network* n, uint32_t sessionid, message* msg)
{
	n->send(sessionid, msg->to_buffer());
}

LuaBind & LuaBind::BindNetwork()
{
	lua.new_usertype<network>("network"
		, sol::call_constructor, sol::no_constructor
		, "async_connect", WRAP_FUNCTION(&network::async_connect)
		, "sync_connect", WRAP_FUNCTION(&network::sync_connect)
		, "close", WRAP_FUNCTION(&network::close)
		, "send", WRAP_FUNCTION(&network_send)
		, "send_message",WRAP_FUNCTION(&network_send_message)
		, "get_session_num", WRAP_FUNCTION(&network::get_session_num)
		);
	return *this;
}

LuaBind & LuaBind::BindHttp()
{
	lua.new_usertype<http_request>("http_request"
		, sol::call_constructor, sol::no_constructor
		, "set_request_type", WRAP_FUNCTION(&http_request::set_request_type)
		, "set_path", WRAP_FUNCTION(&http_request::set_path)
		, "set_content", WRAP_FUNCTION(&http_request::set_content)
		, "push_header", WRAP_FUNCTION(&http_request::push_header)
		);

	lua.new_usertype<http_response>("http_response"
		, sol::call_constructor, sol::no_constructor
		, "content", WRAP_FUNCTION(&http_response::content)
		, "header", WRAP_FUNCTION(&http_response::header)
		, "http_version", WRAP_FUNCTION(&http_response::http_version)
		, "status_code", WRAP_FUNCTION(&http_response::status_code)
		);

	lua.new_usertype<http_client>("http_client"
		, sol::constructors<sol::types<const std::string&>>()
		, "make_request", WRAP_FUNCTION(&http_client::make_request)
		, "request", WRAP_FUNCTION(&http_client::request)
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
