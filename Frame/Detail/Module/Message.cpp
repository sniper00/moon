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
		bool v = CheckFlag(EHeaderFlag::HasSessionID);
		assert(!v);
		if (v)
		{
			return ModuleID();
		}
		return m_Sender;
	}

	void Message::SetSessionID(SessionID id)
	{
		SetFlag(EHeaderFlag::HasSessionID);
		m_Sender = id;
	}

	bool Message::IsSessionID() const
	{
		return CheckFlag(EHeaderFlag::HasSessionID);
	}

	SessionID Message::GetSessionID() const
	{
		assert(CheckFlag(EHeaderFlag::HasSessionID));
		return m_Sender;
	}

	void Message::SetSender(ModuleID moduleID)
	{
		ClearFlag(EHeaderFlag::HasSessionID);
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

	void Message::SetUserID(uint64_t userID)
	{
		m_UserID = userID;
	}

	uint64_t Message::GetUserID() const
	{
		return m_UserID;
	}

	void Message::SetSubUserID(uint64_t subuserID)
	{
		m_SubUserID = subuserID;
	}

	uint64_t Message::GetSubUserID() const
	{
		return m_SubUserID;
	}

	bool Message::HasRPCID() const
	{
		return CheckFlag(EHeaderFlag::HasRPCID);
	}

	void Message::SetRPCID(uint64_t rpcID)
	{
		SetFlag(EHeaderFlag::HasRPCID);
		m_RPCID = rpcID;
	}

	uint64_t Message::GetRPCID() const
	{
		Assert(CheckFlag(EHeaderFlag::HasRPCID),"has no rpc id");
		return m_RPCID;
	}

	bool Message::IsReadOnly() const
	{
		return CheckFlag(EHeaderFlag::ReadOnly);
	}

	void Message::SetReadOnly()
	{
		SetFlag(EHeaderFlag::ReadOnly);
	}

	void Message::SetType(EMessageType type)
	{
		m_Type =(uint8_t)type;
	}

	EMessageType Message::GetType() const
	{
		return (EMessageType)m_Type;
	}

	Message * Message::Clone()
	{
		auto msg = new Message(m_Data);
		msg->m_Flag = m_Flag;
		msg->m_Receiver = m_Receiver;
		msg->m_Sender = m_Sender;
		msg->m_RPCID = m_RPCID;
		msg->m_Type = m_Type;
		msg->m_UserID = m_UserID;
		return msg;
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

	bool Message::CheckFlag(EHeaderFlag flag) const
	{
		return (m_Flag&static_cast<uint8_t>(flag)) != 0;
	}

	void Message::SetFlag(EHeaderFlag flag)
	{
		m_Flag |= (static_cast<uint8_t>(flag));
	}

	void Message::ClearFlag(EHeaderFlag flag)
	{
		m_Flag &= (~(static_cast<uint8_t>(flag)));
	}
}

