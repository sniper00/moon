#include "ModuleLua.h"
#include "Module.h"
#include "MoonNetLuaBind.h"
#include "Message.h"
#include "Common/StringUtils.hpp"
#include "Common/TupleUtils.hpp"
#include "Detail/Log/Log.h"
#include "sol.hpp"

using namespace moon;

struct ModuleLua::ModuleLuaImp
{
	sol::state				lua;
	sol::function			Init;
	sol::function			Start;
	sol::function			Update;
	sol::function			Destory;
	sol::function			OnMessage;
	std::string				Config;
	std::unordered_map<std::string, std::string> KvConfig;
};

ModuleLua::ModuleLua()
	:m_ModuleLuaImp(std::make_unique<ModuleLua::ModuleLuaImp>())
{

}

ModuleLua::~ModuleLua()
{

}

bool ModuleLua::Init(const std::string& config)
{
	m_ModuleLuaImp->Config = config;

	auto vec = string_utils::split<std::string>(config, ";");
	for (auto& it : vec)
	{
		auto pairs = string_utils::split<std::string>(it, ":");
		if (pairs.size() == 2)
		{
			m_ModuleLuaImp->KvConfig.emplace(pairs[0], pairs[1]);
		}
	}

	auto& conf = m_ModuleLuaImp->KvConfig;

	if (!contains_key(conf, "name") || !contains_key(conf, "luafile"))
	{
		CONSOLE_ERROR("Lua moudle with config %s init failed", m_ModuleLuaImp->Config.data());
		return false;
	}

	SetName(conf["name"]);

	return true;
}

void ModuleLua::OnEnter()
{
	sol::state& lua = m_ModuleLuaImp->lua;

	lua.open_libraries(sol::lib::os,sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::io, sol::lib::table, sol::lib::string,sol::lib::debug);
	MoonNetLuaBind luaBind(lua);
	luaBind.BindTime()
		.BindThreadSleep()
		.BindEMessageType()
		.BindMessage()
		.BindMemoryStream()
		.BindBinaryReader()
		.BindBinaryWriter()
		.BindNetworkComponent()
		.BindModule()
		.BindLog()
		.BindUtil();

	auto& conf = m_ModuleLuaImp->KvConfig;
	if (!contains_key(conf, "luafile"))
	{
		CONSOLE_ERROR("Lua moudle [name: %s] Init failed, does not have luafile",conf["name"].data());
		SetOK(false);
		Exit();
		return;
	}
	{
		try
		{
			auto& lua = m_ModuleLuaImp->lua;
			auto& conf = m_ModuleLuaImp->KvConfig;
			lua.set("nativeModule", this);
			sol::object obj = lua.require_file(conf["name"], conf["luafile"]);
			if (!obj.valid())
			{
				CONSOLE_ERROR("Lua moudle [name: %s] Init failed,luafile[%s] load failed", conf["name"].data(), conf["luafile"].data());
				CONSOLE_DEBUG("Traceback: %s", Traceback(m_ModuleLuaImp->lua.lua_state()).data());
				SetOK(false);
				Exit();
				return;
			}

			std::string newmodule = string_utils::format("thisModule = %s.new()", conf["name"].data());
			lua.script(newmodule);
			lua.script(R"(
			function Init(config)
				return thisModule:Init(config)
			end

			function Start()
				return thisModule:Start()
			end

			function Update(interval)
				return thisModule:Update(interval)
			end

			function  Destory()
				return thisModule:Destory()
			end

			function  OnMessage(msg)
				return thisModule:OnMessage(msg)
			end
			)");

			m_ModuleLuaImp->Init = lua["Init"];
			m_ModuleLuaImp->Start = lua["Start"];
			m_ModuleLuaImp->Update = lua["Update"];
			m_ModuleLuaImp->Destory = lua["Destory"];
			m_ModuleLuaImp->OnMessage = lua["OnMessage"];

			Assert(m_ModuleLuaImp->Init.valid() && m_ModuleLuaImp->Start.valid() && m_ModuleLuaImp->Update.valid() && m_ModuleLuaImp->Destory.valid() && m_ModuleLuaImp->OnMessage.valid(), "5 functions!");
			m_ModuleLuaImp->Init(m_ModuleLuaImp->Config);
			m_ModuleLuaImp->Start();
		}
		catch (sol::error& e)
		{
			SetOK(false);
			Exit();
			CONSOLE_ERROR("ModuleLua OnEnter: %s\r\n", e.what());
			CONSOLE_DEBUG("Traceback: %s", Traceback(m_ModuleLuaImp->lua.lua_state()).data());
		}
	}
}

void ModuleLua::OnExit()
{
	if (!IsOk())
		return;

	try
	{
		m_ModuleLuaImp->Destory();
	}
	catch (sol::error& e)
	{
		SetOK(false);
		Exit();
		CONSOLE_ERROR("ModuleLua OnExit: %s\r\n", e.what());
		CONSOLE_DEBUG("Traceback: %s", Traceback(m_ModuleLuaImp->lua.lua_state()).data());
	}
}

void ModuleLua::Update(uint32_t interval)
{
	if (!IsOk())
		return;
	try
	{
		m_ModuleLuaImp->Update(interval);
	}
	catch (sol::error& e)
	{
		SetOK(false);
		Exit();
		CONSOLE_ERROR("ModuleLua Update: %s\r\n", e.what());
		CONSOLE_DEBUG("Traceback: %s", Traceback(m_ModuleLuaImp->lua.lua_state()).data());
	}
}

void ModuleLua::OnMessage(const moon::MessagePtr& msg)
{
	if (!IsOk())
		return;
	try
	{
		m_ModuleLuaImp->OnMessage(msg);
	}
	catch (sol::error& e)
	{
		SetOK(false);
		Exit();
		CONSOLE_ERROR("ModuleLua OnMessage: %s\r\n", e.what());
		CONSOLE_DEBUG("Traceback: %s", Traceback(m_ModuleLuaImp->lua.lua_state()).data());
	}
}

