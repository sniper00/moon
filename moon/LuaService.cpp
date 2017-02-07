#include "LuaService.h"
#include "LuaBind.h"
#include "common/string.hpp"
#include "rapidjson/document.h"
#include "sol.hpp"
#include "log.h"
#include "message.h"
#include "service_pool.h"

#include "components/timer/timer.h"
#include "components/http/http_client.h"
#include "components/tcp/network.h"

using namespace moon;

struct LuaService::Imp
{
	sol::state				lua;
	sol::function			Init;
	sol::function			Start;
	sol::function			Update;
	sol::function			Destory;
	sol::function			OnMessage;
	sol::function			OnNetwork;
	sol::function			OnSystem;
	moon::timer			timer_;
	std::shared_ptr<moon::network> network_;
};

LuaService::LuaService()
	:imp_(std::make_unique<LuaService::Imp>())
{

}

LuaService::~LuaService()
{

}

bool LuaService::init(const std::string& config)
{
	rapidjson::Document doc;
	doc.Parse(config.data());
	if (doc.HasParseError())
	{
		auto r = doc.GetParseError();
		CONSOLE_ERROR("Lua service parse config %s failed,errorcode %d", config.data(),r);
		exit(true);
		return false;
	}

	if (!doc.IsObject())
	{
		doc.SetObject();
	}

	// init network
	do
	{
		if (!doc.HasMember("network"))
		{
			break;
		}

		rapidjson::Value& net = doc["network"];
		if (!net.IsObject())
		{
			break;
		}

		auto&& v = net.GetObject();

		int nthread = 1;
		if (v.HasMember("thread"))
		{
			auto& n = v["thread"];
			if (n.IsInt())
			{
				nthread = n.GetInt();
			}
		}


		std::string nettype;
		if (v.HasMember("type"))
		{
			auto& type = v["type"];
			if (type.IsString())
			{
				nettype = type.GetString();
			}
		}

		uint32_t timeout = 0;
		if (v.HasMember("timeout"))
		{
			auto& t = v["timeout"];
			if (t.IsInt())
			{
				timeout = t.GetInt();
			}
		}

		std::string ip;
		if (v.HasMember("ip"))
		{
			auto& t = v["ip"];
			if (t.IsString())
			{
				ip = t.GetString();
			}
		}

		int port = 0;
		if (v.HasMember("port"))
		{
			auto& t = v["port"];
			if (t.IsInt())
			{
				port = t.GetInt();
			}
		}

		if (ip.empty() || 0 == port)
		{
			CONSOLE_ERROR("Lua service with config %s init network failed, with ip:%s port:%d", config.data(),ip.data(),port);
			exit(true);
			return false;
		}

		imp_->network_ = add_component<moon::network>("network");

		imp_->network_->set_enable_update(true);

		imp_->network_->init(nthread, timeout);

		imp_->network_->set_max_queue_size(4000);

		imp_->network_->set_network_handle([this](const message_ptr_t& m) {
			if (m->broadcast())
			{
				broadcast(m);
			}
			else
			{
				handle_message(m);
			}
		});
		
		if (moon::iequals(nettype, "listen"))
		{
			imp_->network_->listen(ip, std::to_string(port));
		}
		else if (moon::iequals(nettype, "connect"))
		{
			imp_->network_->async_connect(ip, std::to_string(port));
		}
		else
		{
			CONSOLE_ERROR("Lua service init network failed, must have type  'listen' or 'connect' ");
			exit(true);
			return false;
		}
	} while (0);

	if (doc.HasMember("name"))
	{
		rapidjson::Value& v = doc["name"];
		if (v.IsString())
		{
			set_name(v.GetString());
		}
	}

	std::string luafile;
	if (doc.HasMember("file"))
	{
		auto& v = doc["file"];
		if (v.IsString())
		{
			luafile = v.GetString();
		}
	}

	if (luafile.empty())
	{
		CONSOLE_ERROR("Lua service init failed,does not provide lua file ");
		exit(true);
		return false;
	}

	sol::state& lua = imp_->lua;

	lua.open_libraries(sol::lib::os,sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::io, sol::lib::table, sol::lib::string, sol::lib::debug,sol::lib::coroutine);
	
	LuaBind luaBind(lua);
	luaBind.BindTime()
		.BindMessageType()
		.BindService()
		.BindLog()
		.BindUtil()
		.BindPath()
		.BindTimer()
		.BindMessage()
		.BindHttp()
		.BindNetwork();

	{
		try
		{
#if TARGET_PLATFORM == PLATFORM_WINDOWS
			lua.script("package.cpath = './Lib/?.dll;'");
#else
			lua.script("package.cpath = './Lib/?.so;'");
#endif

			lua.set("_service",(service*)this);
			lua.set("Timer", &imp_->timer_);

			sol::object obj = lua.require_file("LUA_SERVICE", luafile);
			if (!obj.valid())
			{
				CONSOLE_ERROR("Lua service Init failed,luafile[%s] load failed",luafile.data());
				CONSOLE_DEBUG("Traceback: %s", Traceback(imp_->lua.lua_state()).data());
				exit(true);
				return false;
			}

			lua.script("thisService = LUA_SERVICE.new()");
			lua.script(R"(

			function __Init(config)
				return thisService:Init(config)
			end

			function __Start()
				thisService:Start()
			end

			function __Update()
				thisService:Update()
			end

			function  __Destory()
				thisService:Destory()
			end

			function __OnMessage(msg,msgtype)
				thisService:OnMessage(msg,msgtype)
			end

			function __OnNetwork(msg,msgtype)
				if thisService.OnNetwork ~= nil then
					thisService:OnNetwork(msg,msgtype)
				end
			end

			function __OnSystem(msg,msgtype)
				if thisService.OnSystem ~= nil then
					thisService:OnSystem(msg,msgtype)
				end
			end
			)");

			imp_->Init = lua["__Init"];
			imp_->Start = lua["__Start"];
			imp_->Update = lua["__Update"];
			imp_->Destory = lua["__Destory"];
			imp_->OnMessage = lua["__OnMessage"];
			imp_->OnNetwork = lua["__OnNetwork"];
			imp_->OnSystem = lua["__OnSystem"];

			Assert(imp_->Init.valid()
				&& imp_->Start.valid()
				&& imp_->Update.valid()
				&& imp_->Destory.valid()
				&& imp_->OnMessage.valid()
				&& imp_->OnNetwork.valid()
				&& imp_->OnSystem.valid()
				,"7 functions!");

			return imp_->Init(config);
		}
		catch (sol::error& e)
		{
			error(moon::format("LuaService Init:%s \n Traceback:%s", e.what(), Traceback(imp_->lua.lua_state()).data()));
		}
	}
	return false;
}

void LuaService::start()
{
	if (exit())
		return;

	service::start();

	try
	{	
		imp_->Start();
	}
	catch (sol::error& e)
	{
		error(moon::format("LuaService start:%s \n Traceback:%s", e.what(), Traceback(imp_->lua.lua_state()).data()));
	}
}

void LuaService::destory()
{
	if (exit())
		return;
	service::destory();

	try
	{
		imp_->Destory();
	}
	catch (sol::error& e)
	{
		error(moon::format("LuaService OnExit:%s \n Traceback:%s", e.what(), Traceback(imp_->lua.lua_state()).data()));
	}
}

void LuaService::on_message(message* msg)
{
	if (exit())
		return;

	try
	{
		auto type = msg->type();

		if ( is_service_message(type))
		{
			imp_->OnMessage(msg, msg->type());
		}
		else if ( is_network_message(type))
		{
			imp_->OnNetwork(msg, msg->type());
		}
		else if (type == message_type::message_system)
		{
			imp_->OnSystem(msg, msg->type());
		}
	}
	catch (sol::error& e)
	{
		error(moon::format("LuaService OnMessage:%s \n Traceback:%s", e.what(), Traceback(imp_->lua.lua_state()).data()));
	}
}

void LuaService::error(const std::string & msg)
{
	auto id = serviceid();
	auto m = message::create();
	auto str = moon::format(R"({"service_id":%u,"name":"%s"})",id,name().data());
	m->to_buffer()->write_back(str.data(), 0, str.size()+1);
	m->set_type(message_type::message_system);
	m->set_userctx("service_crash");
	broadcast(m);
	exit(true);
	log::logstring(true,LogLevel::Error,"native",msg.data());
}

void LuaService::update()
{
	if (exit())
		return;
	service::update();
	try
	{
		imp_->timer_.update();
		if (enable_update())
		{
			imp_->Update();
		}
	}
	catch (sol::error& e)
	{
		error(moon::format("LuaService Update:%s \n Traceback:%s", e.what(), Traceback(imp_->lua.lua_state()).data()));
	}
}


