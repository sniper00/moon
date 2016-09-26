/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include "MacroDefine.h"
#include "Common/MemoryStream.hpp"

namespace moon
{
	//Module消息类型
	enum class EMessageType:uint8_t
	{
		Unknown,
		NetworkData,//网络数据
		NetworkConnect,//网络连接消息
		NetworkClose,//网络断开消息
		NetTypeEnd,
		ModuleData,//Module数据
		ToClient//发送给客户端的数据
	};

	DECLARE_SHARED_PTR(MemoryStream)

	//Module 消息
	class Message
	{
	public:
		using StreamType = MemoryStream;

		Message(size_t capacity = 64, size_t headreserved = 0);

		Message(const MemoryStreamPtr&);

		Message(const Message& msg) = default;

		Message(Message&& msg) = default;

		~Message();

		/**
		* 获取消息的发送者ID，一个消息要么是Module之间通讯的消息，要么是网络消息
		* 参考 GetSessionID
		*/
		ModuleID						GetSender() const;
		void									SetSender(ModuleID moduleID);

		/**
		* 消息的socketID
		*/
		bool									HasSessionID() const;
		SessionID							GetSessionID() const;
		void									SetSessionID(SessionID sessionID);

		/**
		* 设置消息的接收者ID
		*/
		void									SetReceiver(ModuleID moduleID);
		ModuleID						GetReceiver() const;

		/**
		* 
		*/
		bool									HasAccountID() const;
		void									SetAccountID(uint64_t accountID);
		uint64_t							GetAccountID() const;

		/**
		*
		*/
		bool									HasPlayerID() const;
		void									SetPlayerID(uint64_t playerID);
		uint64_t							GetPlayerID() const;

		/**
		*
		*/
		bool									HasSenderRPC() const;
		void									SetSenderRPC(uint64_t senderRPC);
		uint64_t							GetSenderRPC() const;

		/**
		*
		*/
		bool									HasReceiverRPC() const;
		void									SetReceiverRPC(uint64_t receiverRPC);
		uint64_t							GetReceiverRPC() const;

		/**
		* 设置消息类型，参见 EMessageType
		*/
		void									SetType(EMessageType type);

		/**
		* 获取消息类型，参见 EMessageType
		*/
		EMessageType					GetType() const;

		const uint8_t*					Data() const
		{
			return m_Data->Data();
		}

		size_t								Size() const
		{
			return m_Data->Size();
		}

		MemoryStream&			ToStream() const
		{
			return *m_Data;
		}

		MemoryStreamPtr			ToStreamPointer() const
		{
			return m_Data;
		}
	protected:
		bool CheckFlag(uint8_t pos) const;

		void SetFlag(uint8_t pos);

		void ClearFlag(uint8_t pos);

	protected:
		uint8_t																			m_Flag;
		uint8_t																			m_Type;
		mutable ModuleID														m_Sender;
		mutable ModuleID														m_Receiver;
		mutable uint64_t														    m_AccountID;
		mutable uint64_t														    m_PlayerID;
		mutable uint64_t															m_SenderRPCID;
		mutable uint64_t															m_ReceiverRPCID;
		MemoryStreamPtr														m_Data;
	};
};


