#include "MoonNetLuaBind.h"
#include "Common/Time.hpp"
#include "Common/MemoryStream.hpp"
#include "Common/BinaryWriter.hpp"
#include "Common/BinaryReader.hpp"
#include "Common/Path.hpp"
#include "Common/Timer/TimerPool.h"
#include "Message.h"
#include "ObjectCreateHelper.h"

#include "ModuleManager.h"
#include "Module.h"
#include "ModuleLua.h"
#include "Network.h"

#include "Detail/Log/Log.h"

#include "sol.hpp"

using namespace moon;

MoonNetLuaBind::MoonNetLuaBind(sol::state& l)
	:lua(l)
{
}


MoonNetLuaBind::~MoonNetLuaBind()
{
}

MoonNetLuaBind& MoonNetLuaBind::BindTime()
{
	lua.set_function("GetSecond", time::second);
	lua.set_function("GetMillsecond", time::millsecond);

	return *this;
}

MoonNetLuaBind & MoonNetLuaBind::BindTimer()
{
	lua.new_usertype<TimerPool>("TimerPool"
		, sol::call_constructor, sol::no_constructor
		, "ExpiredOnce", &TimerPool::ExpiredOnce
		, "Repeat", &TimerPool::Repeat
		, "Remove", &TimerPool::Remove
		, "StopAllTimer", &TimerPool::StopAllTimer
		, "StartAllTimer", &TimerPool::StartAllTimer
		);

	return *this;
}

MoonNetLuaBind & MoonNetLuaBind::BindUtil()
{
	sol::table tb = lua.create_named_table("Util");
	tb.set_function("HashString", [](const std::string& s) { return std::hash<std::string>()(s);});

	return *this;
}

MoonNetLuaBind & MoonNetLuaBind::BindPath()
{
	sol::table tb = lua.create_named_table("Path");
	tb.set_function("TraverseFolder", Path::TraverseFolder);
	tb.set_function("Exist", Path::Exist);
	tb.set_function("CreateDirectory", Path::CreateDirectory);
	tb.set_function("GetDirectory", Path::GetDirectory);
	tb.set_function("GetExtension", Path::GetExtension);
	tb.set_function("GetName", Path::GetName);
	tb.set_function("GetNameWithoutExtension", Path::GetNameWithoutExtension);
	tb.set_function("GetCurrentDir", Path::GetCurrentDir);
	return *this;
}

MoonNetLuaBind & MoonNetLuaBind::BindLog()
{

	lua.new_enum("LogLevel"
		, "Error", LogLevel::Error
		, "Warn", LogLevel::Warn
		, "Info", LogLevel::Info
		, "Debug", LogLevel::Debug
		, "Trace", LogLevel::Trace
	);

	lua.set_function("LOGV", [](bool bconsole, LogLevel lv, const std::string& content) {
		Log::LogV(bconsole, lv, content.data());
	});
	return *this;
}

MoonNetLuaBind& MoonNetLuaBind::BindThreadSleep()
{
	lua.set_function("Sleep", [](int64_t ms) { thread_sleep(ms); });
	return *this;
}

MoonNetLuaBind& MoonNetLuaBind::BindEMessageType()
{
	lua.new_enum("EMessageType"
		, "Unknown", EMessageType::Unknown
		, "NetworkData", EMessageType::NetworkData
		, "NetworkConnect", EMessageType::NetworkConnect
		, "NetworkClose", EMessageType::NetworkClose
		, "ModuleData", EMessageType::ModuleData
		, "ModuleRPC",EMessageType::ModuleRPC
		, "ToClient", EMessageType::ToClient
	);

	return *this;
}

MoonNetLuaBind& MoonNetLuaBind::BindNetwork()
{
	lua.new_usertype<Network>("Network"
		, sol::call_constructor, sol::no_constructor
		, "InitNet", &Network::InitNet
		, "Listen", &Network::Listen
		, "SyncConnect", &Network::SyncConnect
		, "Connect", &Network::Connect
		, "Send", &Network::Send
		, "Start", &Network::Start
		, "Update", &Network::Update
		, "Destory", &Network::Destory
		, "SetHandler", &Network::SetHandler
		);

	lua.set_function("CreateNetwork", []() {return std::make_shared<Network>();});
	return *this;
}

MoonNetLuaBind& MoonNetLuaBind::BindModule()
{
	lua.new_usertype<ModuleLua>("NativeModule"
		, sol::call_constructor, sol::no_constructor
		, "GetName", &ModuleLua::GetName
		, "GetID", &ModuleLua::GetID
		, "SetEnableUpdate", &ModuleLua::SetEnableUpdate
		, "IsEnableUpdate", &ModuleLua::IsEnableUpdate
		, "Send", &ModuleLua::Send
		, "Broadcast", &ModuleLua::Broadcast
		, "Exit", &ModuleLua::Exit
		);
	return *this;
}

MoonNetLuaBind& MoonNetLuaBind::BindModuleManager()
{
	lua.new_usertype<ModuleManager>("ModuleManager"
		, sol::constructors<sol::types<>>()
		, "Init", &ModuleManager::Init
		, "GetMachineID", &ModuleManager::GetMachineID
		, "CreateModule", &ModuleManager::CreateModule<ModuleLua>
		, "RemoveModule", &ModuleManager::RemoveModule
		, "Send", &ModuleManager::Send
		, "Broadcast", &ModuleManager::Broadcast
		, "Run", &ModuleManager::Run
		, "Stop", &ModuleManager::Stop
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
