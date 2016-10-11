#include "Network.h"
#include "Detail/Network/NetworkFrame.h"
#include "ObjectCreateHelper.h"
#include "Common/SyncQueue.hpp"
#include "Common/TupleUtils.hpp"
#include "Common/BinaryWriter.hpp"
#include "Message.h"

namespace moon
{
	struct Network::NetworkImp
	{
		NetworkImp(int netThreadNum)
		{
			Net = std::make_shared<NetWorkFrame>(make_bind(&NetworkImp::OnNetMessage, this), netThreadNum);
		}

		void OnNetMessage(ESocketMessageType type, SessionID sessionID, const MemoryStreamPtr& data)
		{
			auto msg = new Message(data);
			msg->SetSessionID(sessionID);

			BinaryWriter<MemoryStream> bw(msg->ToStreamPointer().get());
			switch (type)
			{
			case ESocketMessageType::Connect:
			{			
				msg->SetType(EMessageType::NetworkConnect);
				break;
			}
			case ESocketMessageType::RecvData:
			{
				msg->SetType(EMessageType::NetworkData);
				break;
			}
			case ESocketMessageType::Close:
			{
				msg->SetType(EMessageType::NetworkClose);
				break;
			}
			default:
				break;
			}
			NetMsgQueue.PushBack(msg);
		}

		std::shared_ptr<NetWorkFrame>		Net;
		SyncQueue<Message*>						NetMsgQueue;
	};

	Network::Network()
	{
	}

	Network::~Network()
	{
	}

	void Network::InitNet(int netThreadNum)
	{
		m_NetworkImp = std::make_shared<NetworkImp>(netThreadNum);
	}

	bool Network::Listen(const std::string& ip, const std::string& port)
	{
		Assert(ip.size() != 0 && port.size() != 0, "Network::Listen: ip  and port nust not be null");
		Assert(nullptr != m_NetworkImp, "Network::Listen: Network not init");
		m_NetworkImp->Net->Listen(ip, port);
		return (m_NetworkImp->Net->GetErrorCode() == 0);
	}

	void Network::Connect(const std::string& ip, const std::string& port)
	{
		Assert(nullptr != m_NetworkImp, "Network::Connect: Network not init");
		m_NetworkImp->Net->AsyncConnect(ip, port);
	}

	SessionID Network::SyncConnect(const std::string & ip, const std::string & port)
	{
		Assert(nullptr != m_NetworkImp, "Network::SyncConnect: Network not init");
		return m_NetworkImp->Net->SyncConnect(ip, port);
	}

	void Network::SendNetMessage(SessionID sessionID, Message* msg)
	{
		Assert(nullptr != m_NetworkImp, "Network::SendNetMessage: Network not init");
		m_NetworkImp->Net->Send(sessionID, msg->ToStreamPointer());
		SAFE_DELETE(msg);
	}

	void Network::Close(SessionID sessionID)
	{
		Assert(nullptr != m_NetworkImp, "Network::Close: Network not init");

		m_NetworkImp->Net->CloseSession(sessionID, ESocketState::ForceClose);
	}

	void Network::Start()
	{
		if (nullptr == m_NetworkImp)
			return;

		m_NetworkImp->Net->Run();
	}

	void Network::Update(uint32_t interval)
	{
		if (nullptr == m_NetworkImp)
			return;

		if (OnNetMessage != nullptr)
		{
			auto msgs = m_NetworkImp->NetMsgQueue.Move();
			for (auto& it : msgs)
			{
				OnNetMessage(it);
			}
		}
	}

	void Network::Destory()
	{
		if (nullptr == m_NetworkImp)
			return;
		m_NetworkImp->NetMsgQueue.Exit();
		m_NetworkImp->Net->Stop();
	}

}

