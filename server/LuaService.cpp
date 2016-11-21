#include "LuaService.h"
#include "LuaBind.h"
#include "timer.h"
#include "sol.hpp"
#include "common/string.hpp"
#include "log.h"
#include "message.h"

using namespace moon;

struct LuaService::Imp
{
	sol::state				lua;
	sol::function			Init;
	sol::function			Start;
	sol::function			Update;
	sol::function			Destory;
	sol::function			OnMessage;
	sol::function			ToClientData;
	moon::timer			timer_;
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
	if (!service::init(config))
	{
		exit(true);
		return false;
	}

	auto name = get_config("name");
	auto luafile = get_config("luafile");

	auto netthread = get_config("netthread");
	auto timeout = get_config("timeout");

	if (name.empty() || luafile.empty())
	{
		CONSOLE_ERROR("Lua service with config %s init failed", config.data());
		exit(true);
		return false;
	}

	if (!netthread.empty())
	{
		init_net(std::stoi(netthread), std::stoi(timeout));
	}

	set_name(name);

	sol::state& lua = imp_->lua;

	lua.open_libraries(sol::lib::os, sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::io, sol::lib::table, sol::lib::string, sol::lib::debug);
	LuaBind luaBind(lua);
	luaBind.BindTime()
		.BindMessageType()
		.BindService()
		.BindLog()
		.BindUtil()
		.BindPath()
		.BindTimer()
		.BindMessage();

	{
		try
		{
			auto& lua = imp_->lua;

#if TARGET_PLATFORM == PLATFORM_WINDOWS
			lua.script("package.cpath = './Lib/?.dll;'");
#else
			lua.script("package.cpath = './Lib/?.so;'");
#endif

			lua.set("nativeService", this);
			lua.set("Timer", std::ref(imp_->timer_));

			sol::object obj = lua.require_file(name, luafile);
			if (!obj.valid())
			{
				CONSOLE_ERROR("Lua service [name: %s] Init failed,luafile[%s] load failed", name.data(), luafile.data());
				CONSOLE_DEBUG("Traceback: %s", Traceback(imp_->lua.lua_state()).data());
				exit(true);
				return false;
			}

			std::string newmodule = moon::format("thisService = %s.new()", name.data());
			lua.script(newmodule);
			lua.script(R"(
			function Init(config)
				return thisService:Init(config)
			end

			function Start()
				return thisService:Start()
			end

			function Update()
				return thisService:Update()
			end

			function  Destory()
				return thisService:Destory()
			end

			function  OnMessage(msg,msgtype)
				return thisService:OnMessage(msg,msgtype)
			end

			function  ToClientData(msg)
				if thisService.ToClientData ~= nil then
					thisService:ToClientData(msg)
				end
			end
			)");

			imp_->Init = lua["Init"];
			imp_->Start = lua["Start"];
			imp_->Update = lua["Update"];
			imp_->Destory = lua["Destory"];
			imp_->OnMessage = lua["OnMessage"];
			imp_->ToClientData = lua["ToClientData"];
			Assert(imp_->Init.valid()
				&& imp_->Start.valid()
				&& imp_->Update.valid()
				&& imp_->Destory.valid()
				&& imp_->OnMessage.valid()
				&& imp_->ToClientData.valid(), "5 functions!");

			imp_->Init(config);
			return true;
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

	if (msg->type() == MTYPE_CLIENT)
	{
		try
		{
			imp_->ToClientData(msg);
		}
		catch (sol::error& e)
		{
			error(moon::format("LuaService ToClientData:%s \n Traceback:%s", e.what(), Traceback(imp_->lua.lua_state()).data()));
		}
	}
	else
	{
		try
		{
			imp_->OnMessage(msg,msg->type());
		}
		catch (sol::error& e)
		{
			error(moon::format("LuaService OnMessage:%s \n Traceback:%s", e.what(), Traceback(imp_->lua.lua_state()).data()));
		}
	}
}

void LuaService::error(const std::string & msg)
{
	auto id = serviceid();
	buffer buf;
	buf.write_back(&id,0,1);
	buf.write_back(name().data(), 0, name().size() + 1);
	//broadcast(std::string((char*)buf.data(), buf.size()));
	exit(true);
	log::logstring(true,LogLevel::Error,"native",msg.data());
}

service_ptr_t LuaService::clone()
{
	return std::make_shared<LuaService>();
}

void LuaService::update()
{
	if (exit())
		return;
	service::update();
	try
	{
		service::update();
		imp_->timer_.update();
		imp_->Update();
	}
	catch (sol::error& e)
	{
		error(moon::format("LuaService Update:%s \n Traceback:%s", e.what(), Traceback(imp_->lua.lua_state()).data()));
	}
}


