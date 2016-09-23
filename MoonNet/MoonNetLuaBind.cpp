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

	//sol::table tb = lua.create_named_table("Log");
	//tb.set_function("Trace", [](const std::string& str) { LOG_TRACE(str.data());});
	//tb.set_function("Debug", [](const std::string& str) { LOG_DEBUG(str.data());});
	//tb.set_function("Info", [](const std::string& str) { LOG_INFO(str.data());});
	//tb.set_function("Warn", [](const std::string& str) { LOG_WARN(str.data());});
	//tb.set_function("Error", [](const std::string& str) { LOG_ERROR(str.data());});

	//tb.set_function("ConsoleTrace", [](const std::string& str) { CONSOLE_TRACE(str.data());});
	//tb.set_function("ConsoleDebug", [](const std::string& str) { CONSOLE_DEBUG(str.data());});
	//tb.set_function("ConsoleInfo", [](const std::string& str) { CONSOLE_INFO(str.data());});
	//tb.set_function("ConsoleWarn", [](const std::string& str) { CONSOLE_WARN(str.data());});
	//tb.set_function("ConsoleError", [](const std::string& str) { CONSOLE_ERROR(str.data());});
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
	using MessageWriter = BinaryWriter<Message>;
	lua.new_usertype<MessageWriter>("MessageWriter"
		, sol::constructors<sol::types<const MessageWriter::TPointer&>>()
		, "Size", &MessageWriter::Size
		, "WriteString", &MessageWriter::Write<std::string>
		, "WriteInt8", &MessageWriter::Write<int8_t>
		, "WriteUint8", &MessageWriter::Write<uint8_t>
		, "WriteInt16", &MessageWriter::Write<int16_t>
		, "WriteUint16", &MessageWriter::Write<uint16_t>
		, "WriteInt32", &MessageWriter::Write<int32_t>
		, "WriteUint32", &MessageWriter::Write<uint32_t>
		, "WriteInt64", &MessageWriter::Write<int64_t>
		, "WriteUint64", &MessageWriter::Write<uint64_t>
		, "WriteInteger", &MessageWriter::Write<int64_t>
		, "WriteFloat", &MessageWriter::Write<float>
		, "WriteDouble", &MessageWriter::Write<double>
		, "WriteNumber", &MessageWriter::Write<double>
		);

	return *this;
}

MoonNetLuaBind& MoonNetLuaBind::BindBinaryReader()
{
	using MessageReader = BinaryReader<Message>;
	lua.new_usertype<MessageReader>("MessageReader"
		, sol::constructors<sol::types<const MessageReader::TPointer&>>()
		, "Size", &MessageReader::Size
		, "Bytes", &MessageReader::Bytes
		, "ReadString", &MessageReader::ReadString
		, "ReadInt8", &MessageReader::Read<int8_t>
		, "ReadUint8", &MessageReader::Read<uint8_t>
		, "ReadInt16", &MessageReader::Read<int16_t>
		, "ReadUint16", &MessageReader::Read<uint16_t>
		, "ReadInt32", &MessageReader::Read<int32_t>
		, "ReadUint32", &MessageReader::Read<uint32_t>
		, "ReadInt64", &MessageReader::Read<int64_t>
		, "ReadUint64", &MessageReader::Read<uint64_t>
		, "ReadInteger", &MessageReader::Read<int64_t>
		, "ReadFloat", &MessageReader::Read<float>
		, "ReadDouble", &MessageReader::Read<double>
		, "ReadNumber", &MessageReader::Read<double>
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
		, "NetTypeEnd", EMessageType::NetTypeEnd
		, "ModuleData", EMessageType::ModuleData
		, "ToClient", EMessageType::ToClient
	);

	return *this;
}

MoonNetLuaBind& MoonNetLuaBind::BindMessage()
{
	lua.new_usertype<Message>("Message"
		, sol::call_constructor, sol::no_constructor
		, "HasSessionID", &Message::HasSessionID
		, "GetSessionID", &Message::GetSessionID
		, "SetSessionID", &Message::SetSessionID

		, "GetSender", &Message::GetSender
		, "SetSender", &Message::SetSender
		, "SetReceiver", &Message::SetReceiver
		, "GetReceiver", &Message::GetReceiver

		, "HasAccountID", &Message::HasAccountID
		, "SetAccountID", &Message::SetAccountID
		, "GetAccountID", &Message::GetAccountID

		, "HasPlayerID", &Message::HasPlayerID
		, "SetPlayerID", &Message::SetPlayerID
		, "GetPlayerID", &Message::GetPlayerID

		, "HasSenderRPC", &Message::HasSenderRPC
		, "SetSenderRPC", &Message::SetSenderRPC
		, "GetSenderRPC", &Message::GetSenderRPC

		, "HasReceiverRPC", &Message::HasReceiverRPC
		, "SetReceiverRPC", &Message::SetReceiverRPC
		, "GetReceiverRPC", &Message::GetReceiverRPC
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               
		, "SetType", &Message::SetType
		, "GetType", &Message::GetType
		, "Size", &Message::Size

		);

	lua.set_function("CreateMessage", [](size_t size = 64) {
		return ObjectCreateHelper<Message>::Create(size);
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
		, "SendMessage", &ModuleLua::SendMessage
		, "BroadcastMessage", &ModuleLua::BroadcastMessage
		, "PushMessage", &ModuleLua::PushMessage
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
		, "SendMessage", &ModuleManager::SendMessage
		, "BroadcastMessage", &ModuleManager::BroadcastMessage
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
