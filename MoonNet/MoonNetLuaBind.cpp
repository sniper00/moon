#include "MoonNetLuaBind.h"
#include "Common/Time.hpp"
#include "Common/MemoryStream.hpp"
#include "Common/BinaryWriter.hpp"
#include "Common/BinaryReader.hpp"
#include "Common/Path.hpp"

#include "Message.h"
#include "ObjectCreateHelper.h"
#include "Module.h"
#include "ModuleLua.h"
#include "ModuleManager.h"
#include "NetworkComponent.h"

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

MoonNetLuaBind& MoonNetLuaBind::BindMemoryStream()
{
	lua.new_usertype<MemoryStream>("MemoryStream"
		, sol::call_constructor, sol::no_constructor
		, "Size", &MemoryStream::Size
		, "Clear", &MemoryStream::Clear
		);

	lua.set_function("CreateMemoryStream", [](size_t size) {
		return ObjectCreateHelper<MemoryStream>::Create(size);
	});

	return *this;
}

MoonNetLuaBind& MoonNetLuaBind::BindBinaryWriter()
{
	using Type = BinaryWriter<Message>;
	lua.new_usertype<Type>("MessageWriter"
		, sol::constructors<sol::types<Type::TPointer>>()
		, "Size", &Type::Size
		, "WriteString", &Type::Write<std::string>
		, "WriteInt8", &Type::Write<int8_t>
		, "WriteUint8", &Type::Write<uint8_t>
		, "WriteInt16", &Type::Write<int16_t>
		, "WriteUint16", &Type::Write<uint16_t>
		, "WriteInt32", &Type::Write<int32_t>
		, "WriteUint32", &Type::Write<uint32_t>
		, "WriteInt64", &Type::Write<int64_t>
		, "WriteUint64", &Type::Write<uint64_t>
		, "WriteInteger", &Type::Write<int64_t>
		, "WriteFloat", &Type::Write<float>
		, "WriteDouble", &Type::Write<double>
		, "WriteNumber", &Type::Write<double>
		);

	return *this;
}

MoonNetLuaBind& MoonNetLuaBind::BindBinaryReader()
{
	using Type = BinaryReader<Message>;
	lua.new_usertype<Type>("MessageReader"
		, sol::constructors<sol::types<Type::TStreamPointer>>()
		, "Size", &Type::Size
		, "Bytes", &Type::Bytes
		, "ReadString", &Type::Read<std::string>
		, "ReadInt8", &Type::Read<int8_t>
		, "ReadUint8", &Type::Read<uint8_t>
		, "ReadInt16", &Type::Read<int16_t>
		, "ReadUint16", &Type::Read<uint16_t>
		, "ReadInt32", &Type::Read<int32_t>
		, "ReadUint32", &Type::Read<uint32_t>
		, "ReadInt64", &Type::Read<int64_t>
		, "ReadUint64", &Type::Read<uint64_t>
		, "ReadInteger", &Type::Read<int64_t>
		, "ReadFloat", &Type::Read<float>
		, "ReadDouble", &Type::Read<double>
		, "ReadNumber", &Type::Read<double>
		);

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

MoonNetLuaBind& MoonNetLuaBind::BindMessage()
{
	lua.new_usertype<Message>("Message"
		, sol::call_constructor, sol::no_constructor
		, "HasSessionID", &Message::IsSessionID
		, "GetSessionID", &Message::GetSessionID
		, "GetSender", &Message::GetSender
		, "SetSender", &Message::SetSender
		, "SetReceiver", &Message::SetReceiver
		, "GetReceiver", &Message::GetReceiver
		, "SetAccountID", &Message::SetAccountID
		, "GetAccountID", &Message::GetAccountID
		, "HasPlayerID", &Message::IsPlayerID
		, "SetPlayerID", &Message::SetPlayerID
		, "GetPlayerID", &Message::GetPlayerID
		, "SetRPCID", &Message::SetRPCID
		, "GetRPCID", &Message::GetRPCID
		, "HasRPCID", &Message::HasRPCID
		, "SetType", &Message::SetType
		, "GetType", &Message::GetType
		, "Size", &Message::Size
		, "Clone",&Message::Clone
		);

	lua.set_function("CreateMessage", []() {
		return  new Message();
	});

	lua.set_function("CreateMessageWithSize", [](size_t size) {
		return  new Message(size);
	});

	return *this;
}

MoonNetLuaBind& MoonNetLuaBind::BindNetworkComponent()
{
	lua.new_usertype<NetworkComponent>("NetworkComponent"
		, sol::call_constructor, sol::no_constructor
		, "InitNet", &NetworkComponent::InitNet
		, "Listen", &NetworkComponent::Listen
		, "SyncConnect", &NetworkComponent::SyncConnect
		, "Connect", &NetworkComponent::Connect
		, "SendNetMessage", &NetworkComponent::SendNetMessage
		, "OnEnter", &NetworkComponent::OnEnter
		, "Update", &NetworkComponent::Update
		, "OnExit", &NetworkComponent::OnExit
		, "OnHandleNetMessage", &NetworkComponent::OnNetMessage
		);

	lua.set_function("CreateNetwork", []() {return std::make_shared<NetworkComponent>();});
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
