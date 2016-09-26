/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#include "Session.h"
#include "NetworkService.h"
#include "Detail/Log/Log.h"
#include "Common/BinaryWriter.hpp"
#include "Common/TupleUtils.hpp"

namespace moon
{
	Session::Session(NetMessageDelegate& netMessageDelegate,NetworkService& networkService)
		:m_Delegate(netMessageDelegate)
		,m_Service(networkService)
		,m_Socket(networkService.GetIoService())
		,m_RecvMemoryStream(IO_BUFFER_SIZE)
		,m_SendMemoryStream(IO_BUFFER_SIZE)
		, m_IsSending(false)
		,m_State(ESocketState::Ok)	
	{
		LOG_TRACE("Create Session");
	}

	Session::~Session(void)
	{
		LOG_TRACE("Release Session %u state[%d]", GetID(), (int)m_State);
	}

	bool Session::Start()
	{
		ParseRemoteEndPoint();

		if (!IsOk())
		{
			OnClose();
			return false;
		}

		RefreshLastRecevieTime();

		OnConnect();
	
		PostRead();
		return true;
	}

	void Session::Close(ESocketState  state)
	{
		if (m_Socket.is_open())
		{
			LOG_TRACE("Session address[%s] forced closed, state[%d]", GetRemoteIP().c_str(), (int)state);
			m_State = state;
			//所有异步处理将会立刻调用，并触发 asio::error::operation_aborted
			m_Socket.shutdown(asio::ip::tcp::socket::shutdown_both, m_ErrorCode);
			if (m_ErrorCode)
			{
				CONSOLE_TRACE("Session address[%s] shutdown failed:%s.", GetRemoteIP().c_str(), m_ErrorCode.message().c_str());
			}
			m_Socket.close(m_ErrorCode);
			if (m_ErrorCode)
			{
				CONSOLE_TRACE("Session address[%s] close failed:%s.", GetRemoteIP().c_str(), m_ErrorCode.message().c_str());
			}
			LOG_TRACE("Session address[%s] close success.", GetRemoteIP().c_str());
		}
	}

	void Session::PostRead()
	{
		if (!IsOk())
		{
			return;
		}

		m_Socket.async_read_some(
			asio::buffer(m_RecvBuffer),
			make_bind(&Session::HandleRead, shared_from_this())
		);
	}

	void Session::HandleRead(const asio::error_code& e, std::size_t bytes_transferred)
	{
		//receive data error
		if (e)
		{
			m_ErrorCode = e;
			OnClose();
			return;
		}

		//client close
		if (0 == bytes_transferred)
		{
			m_State = ESocketState::ClientClose;
			m_ErrorCode = e;
			OnClose();
			return;
		}

		RefreshLastRecevieTime();
		m_RecvMemoryStream.WriteBack(m_RecvBuffer, 0, bytes_transferred);

		while (m_RecvMemoryStream.Size() > sizeof(msg_size_t))
		{
			msg_size_t size = *(msg_size_t*)m_RecvMemoryStream.Data();

			//check Message len
			assert(size <= MAX_MSG_SIZE);
			if (size > MAX_MSG_SIZE)
			{
				m_State = ESocketState::IllegalDataLength;
				OnClose();
				return;
			}

			//消息不完整 继续接收消息
			if (m_RecvMemoryStream.Size() < sizeof(msg_size_t) + size)
			{
				break;
			}

			m_RecvMemoryStream.Seek(sizeof(msg_size_t), MemoryStream::Current);
			OnMessage(m_RecvMemoryStream.Data(), size);
			m_RecvMemoryStream.Seek(size, MemoryStream::Current);
		}

		PostRead();	
	}

	void Session::PostSend()
	{
		if (!IsOk())
		{
			OnClose();
			return;
		}
		
		if (m_SendQueue.size() == 0)
			return;

		m_SendMemoryStream.Clear();

		while ((m_SendQueue.size() > 0) && (m_SendMemoryStream.Size() + m_SendQueue.front()->Size() <= IO_BUFFER_SIZE))
		{
			auto& msg = m_SendQueue.front();
			m_SendMemoryStream.WriteBack(msg->Data(),0,msg->Size());
			m_SendQueue.pop_front();
		}

		if (0 == m_SendMemoryStream.Size())
		{
			CONSOLE_TRACE("Temp to send to %s  0 bytes Message.", GetRemoteIP().c_str());
			return;
		}

		m_IsSending = true;

		asio::async_write(
			m_Socket,
			asio::buffer(m_SendMemoryStream.Data(), m_SendMemoryStream.Size()),
			make_bind(&Session::HandleSend, shared_from_this())
			);
	}

	void Session::HandleSend(const asio::error_code& e, std::size_t bytes_transferred)
	{
		m_IsSending = false;
		if (!e)
		{
			PostSend();
			return;
		}
		m_ErrorCode = e;
		OnClose();
	}

	bool Session::IsOk()
	{
		if (m_ErrorCode || m_State != ESocketState::Ok)
		{
			return false;
		}
		return true;
	}

	void Session::Send(const MemoryStreamPtr& msg)
	{
		if (msg->Size() > MAX_MSG_SIZE)
		{
			CONSOLE_TRACE("Warning: try to send %lluByte message, the max limit is %lluByte, this message will not send!", msg->Size(),MAX_MSG_SIZE);
			return;
		}

		{
			msg_size_t msgsize = static_cast<msg_size_t>(msg->Size());
			size_t buffsize = (msgsize > (IO_BUFFER_SIZE - sizeof(msg_size_t)))? (IO_BUFFER_SIZE - sizeof(msg_size_t)) : msgsize;
			MemoryStreamPtr splitmsg = ObjectCreateHelper<MemoryStream>::Create(buffsize+ sizeof(msg_size_t));		
			splitmsg->WriteBack(&msgsize, 0,1);
			splitmsg->WriteBack(msg->Data(), 0, buffsize);
			msg->Seek(static_cast<int>(buffsize), MemoryStream::seek_origin::Current);
			m_SendQueue.emplace_back(splitmsg);
		}

		while (msg->Size() > 0)
		{
			msg_size_t msgsize = static_cast<msg_size_t>(msg->Size());
			size_t buffsize = (msgsize > IO_BUFFER_SIZE) ? IO_BUFFER_SIZE: msgsize;
			MemoryStreamPtr splitmsg = ObjectCreateHelper<MemoryStream>::Create(buffsize);
			splitmsg->WriteBack(msg->Data(), 0, buffsize);
			msg->Seek(static_cast<int>(buffsize), MemoryStream::seek_origin::Current);
			m_SendQueue.emplace_back(splitmsg);
		}

		if (msg->Size() != 0)
		{
			m_SendQueue.push_back(msg);
		}

		if (!m_IsSending)
		{
			PostSend();
		}
	}

	void Session::ParseRemoteEndPoint()
	{
		std::string		 straddress;
		do
		{
			auto ep = m_Socket.remote_endpoint(m_ErrorCode);
			BREAK_IF(m_ErrorCode);
			auto adr = ep.address();
			auto port = ep.port();
			m_RemoteIP = adr.to_string(m_ErrorCode);
			m_RemotePort= port;
			BREAK_IF(m_ErrorCode);
		} while (0);
	}

	void Session::RefreshLastRecevieTime()
	{
		m_LastRecevieTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	void Session::OnConnect()
	{
		MemoryStreamPtr ms = ObjectCreateHelper<MemoryStream>::Create(64);
		BinaryWriter<MemoryStream> bw(ms.get());
		bw << GetRemoteIP();
		bw << GetRemotePort();
		bw << (uint16_t)m_State;
		bw << m_ErrorCode.message();
		m_Delegate(ESocketMessageType::Connect, GetID(), ms);
	}

	void Session::OnClose()
	{
		MemoryStreamPtr ms = ObjectCreateHelper<MemoryStream>::Create(64);
		BinaryWriter<MemoryStream> bw(ms.get());
		bw << GetRemoteIP();
		bw << GetRemotePort();
		bw << (uint16_t)m_State;
		bw << m_ErrorCode.message();
		m_Delegate(ESocketMessageType::Close, GetID(), ms);
		m_Service.RemoveSession(GetID());
	}

	void Session::OnMessage(const uint8_t* data, size_t len)
	{
		MemoryStreamPtr msd = ObjectCreateHelper<MemoryStream>::Create(len);
		msd->WriteBack(data, 0, len);
		m_Delegate(ESocketMessageType::RecvData, GetID(), msd);
	}
};
