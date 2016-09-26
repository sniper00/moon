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
enum class EHeaderFlag
{
	HasSessionID = 1 << 0,
	HasAccountID = 1 << 1,
	HasPlayerID = 1 << 2,
	HasSenderRPC = 1<<3,
	HasReceiverRPC = 1<<4
};

namespace moon
{
	Message::Message(size_t capacity, size_t headreserved)
	{
		m_Type = (uint8_t)EMessageType::Unknown;
		m_Flag = 0;
		m_Receiver = 0;
		m_Sender = 0;
		m_ReceiverRPCID = 0;
		m_SenderRPCID = 0;
		m_AccountID = 0;
		m_PlayerID = 0;

		m_Data = ObjectCreateHelper<MemoryStream>::Create(capacity, headreserved);

		//CONSOLE_TRACE("message new %x",(size_t)(this));
	}

	Message::Message(const MemoryStreamPtr& msd)
	{
		m_Data = msd;
		m_Type = (uint8_t)EMessageType::Unknown;
		m_Flag = 0;
		m_Receiver = 0;
		m_Sender = 0;
		m_ReceiverRPCID = 0;
		m_SenderRPCID = 0;
		m_AccountID = 0;
		m_PlayerID = 0;
		//CONSOLE_TRACE("message new %x with ms", (size_t)(this));
	}

	Message::~Message()
	{
		//CONSOLE_TRACE("message release %x", (size_t)(this));
	}

	ModuleID Message::GetSender() const
	{
		bool v = CheckFlag((uint8_t)EHeaderFlag::HasSessionID);
		assert(!v);
		if (v)
		{
			return ModuleID();
		}
		return m_Sender;
	}

	bool Message::HasSessionID() const
	{
		return CheckFlag((uint8_t)EHeaderFlag::HasSessionID);
	}

	SessionID Message::GetSessionID() const
	{
		bool v = CheckFlag((uint8_t)EHeaderFlag::HasSessionID);
		assert(v);
		if (!v)
		{
			return SessionID();
		}
		return  m_Sender;
	}

	void Message::SetSender(ModuleID moduleID)
	{
		ClearFlag((uint8_t)EHeaderFlag::HasSessionID);
		m_Sender = moduleID;
	}

	void Message::SetSessionID(SessionID sessionID)
	{
		SetFlag((uint8_t)EHeaderFlag::HasSessionID);
		m_Sender = sessionID;
	}

	void Message::SetReceiver(ModuleID receiver)
	{
		m_Receiver = receiver;
	}

	ModuleID Message::GetReceiver() const
	{
		return m_Receiver;
	}

	bool Message::HasAccountID() const
	{
		return CheckFlag((uint8_t)EHeaderFlag::HasAccountID);
	}

	void Message::SetAccountID(uint64_t accountID)
	{
		SetFlag((uint8_t)EHeaderFlag::HasAccountID);
		m_AccountID = accountID;
	}

	uint64_t Message::GetAccountID() const
	{
		assert(CheckFlag((uint8_t)EHeaderFlag::HasAccountID));
		return m_AccountID;
	}

	bool Message::HasPlayerID() const
	{
		return CheckFlag((uint8_t)EHeaderFlag::HasPlayerID);
	}

	void Message::SetPlayerID(uint64_t playerID)
	{
		SetFlag((uint8_t)EHeaderFlag::HasPlayerID);
		m_PlayerID = playerID;
	}

	uint64_t Message::GetPlayerID() const
	{
		assert(CheckFlag((uint8_t)EHeaderFlag::HasPlayerID));
		return m_PlayerID;
	}

	bool Message::HasSenderRPC() const
	{
		return CheckFlag((uint8_t)EHeaderFlag::HasSenderRPC);
	}

	void Message::SetSenderRPC(uint64_t senderRPC)
	{
		SetFlag((uint8_t)EHeaderFlag::HasSenderRPC);
		m_SenderRPCID = senderRPC;
	}

	uint64_t Message::GetSenderRPC() const
	{
		CheckFlag((uint8_t)EHeaderFlag::HasSenderRPC);
		return m_SenderRPCID;
	}

	bool Message::HasReceiverRPC() const
	{
		return CheckFlag((uint8_t)EHeaderFlag::HasReceiverRPC);
	}

	void Message::SetReceiverRPC(uint64_t receiverRPC)
	{
		SetFlag((uint8_t)EHeaderFlag::HasReceiverRPC);
		m_ReceiverRPCID = receiverRPC;
	}

	uint64_t Message::GetReceiverRPC() const
	{
		CheckFlag((uint8_t)EHeaderFlag::HasReceiverRPC);
		return m_ReceiverRPCID;
	}

	void Message::SetType(EMessageType type)
	{
		m_Type =(uint8_t)type;
	}

	EMessageType Message::GetType() const
	{
		return (EMessageType)m_Type;
	}
	bool Message::CheckFlag(uint8_t pos) const
	{
		return (m_Flag& pos) != 0;
	}
	void Message::SetFlag(uint8_t pos)
	{
		m_Flag |= (pos);
	}
	void Message::ClearFlag(uint8_t pos)
	{
		m_Flag &= (~(pos));
	}
}

