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
	enum class EMessageType :uint8_t
	{
		Unknown,
		NetworkData,//网络数据
		NetworkConnect,//网络连接消息
		NetworkClose,//网络断开消息
		ModuleData,//Module数据
		ModuleRPC,//远程调用消息
		ToClient//发送给客户端的数据
	};

	enum class EHeaderFlag:uint8_t
	{
		HasSessionID	= 1 << 0,
		HasRPCID			= 1 << 1,
		ReadOnly			= 1 << 2
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

		void									SetSender(ModuleID moduleID);
		ModuleID							GetSender() const;

		void									SetSessionID(SessionID id);
		bool									IsSessionID() const;
		SessionID							GetSessionID() const;

		void									SetReceiver(ModuleID moduleID);
		ModuleID							GetReceiver() const;

		void									SetUserID(uint64_t userid);
		uint64_t							GetUserID() const;

		void									SetSubUserID(uint64_t subUserID);
		uint64_t							GetSubUserID() const;

		bool									HasRPCID() const;
		void									SetRPCID(uint64_t rpcID);
		uint64_t							GetRPCID() const;

		bool									IsReadOnly() const;
		void									SetReadOnly();

		/**
		* 设置消息类型，参见 EMessageType
		*/
		void									SetType(EMessageType type);
		EMessageType					GetType() const;

	
		const uint8_t*					Data() const
		{
			return m_Data->Data();
		}

		std::string							Bytes() const
		{
			return std::string((char*)Data(), Size());
		}

		void									WriteData(const std::string& v)
		{
			m_Data->WriteBack(v.data(),0, v.size());
		}

		size_t								Size() const
		{
			return m_Data->Size();
		}

		MemoryStreamPtr			ToStreamPointer() const
		{
			return m_Data;
		}

		Message*							Clone();

	protected:
		void									Init();

		bool									CheckFlag(EHeaderFlag) const;

		void									SetFlag(EHeaderFlag);

		void									ClearFlag(EHeaderFlag);
	protected:
		uint8_t								m_Flag;
		uint8_t								m_Type;
		ModuleID							m_Sender;
		ModuleID							m_Receiver;
		uint64_t							m_UserID;
		uint64_t							m_SubUserID;
		uint64_t							m_RPCID;
		MemoryStreamPtr			m_Data;
	};
};


