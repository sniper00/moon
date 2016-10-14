/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#include "Message.h"
#include "ModuleManager.h"
#include "ObjectCreateHelper.h"
#include "Detail/Log/Log.h"


namespace moon
{
	Message::Message(size_t capacity, size_t headreserved)
	{
		Init();
		m_Data = ObjectCreateHelper<MemoryStream>::Create(capacity, headreserved);
		//CONSOLE_TRACE("message new %x",(size_t)(this));
	}

	Message::Message(const MemoryStreamPtr& ms)
	{
		Init();

		m_Data = ms;
		//CONSOLE_TRACE("message new %x with ms", (size_t)(this));
	}

	Message::~Message()
	{
		//CONSOLE_TRACE("message release %x", (size_t)(this));
	}

	ModuleID Message::GetSender() const
	{
		return m_Sender;
	}

	void Message::SetSender(ModuleID moduleID)
	{
		m_Sender = moduleID;
	}

	void Message::SetReceiver(ModuleID receiver)
	{
		m_Receiver = receiver;
	}

	ModuleID Message::GetReceiver() const
	{
		return m_Receiver;
	}

	void Message::SetUserData(const std::string & userdata)
	{
		m_UserData = std::move(userdata);
	}

	const std::string & Message::GetUserData() const
	{
		return m_UserData;
	}

	void Message::SetRPCID(uint64_t rpcID)
	{
		m_RPCID = rpcID;
	}

	uint64_t Message::GetRPCID() const
	{
		return m_RPCID;
	}

	void Message::SetType(EMessageType type)
	{
		m_Type =(uint8_t)type;
	}

	EMessageType Message::GetType() const
	{
		return (EMessageType)m_Type;
	}

	void Message::Init()
	{
		m_Type = (uint8_t)EMessageType::Unknown;
		m_Flag = 0;
		m_Sender = 0;
		m_Receiver = 0;
		m_UserID = 0;
		m_SubUserID = 0;
		m_RPCID = 0;
	}
}

