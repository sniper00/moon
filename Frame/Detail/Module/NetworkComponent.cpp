#include "NetworkComponent.h"
#include "Detail/Network/NetworkFrame.h"
#include "ObjectCreateHelper.h"
#include "Common/SyncQueue.hpp"
#include "Common/TupleUtils.hpp"
#include "Common/BinaryWriter.hpp"
#include "Message.h"

namespace moon
{
	struct NetworkComponent::NetworkComponentImp
	{
		NetworkComponentImp(int netThreadNum)
		{
			Net = std::make_shared<NetWorkFrame>(make_bind(&NetworkComponentImp::OnNetMessage, this), netThreadNum);
		}

		void OnNetMessage(ESocketMessageType type, SessionID sessionID, const MemoryStreamPtr& data)
		{
			auto msg = new Message(data);
			msg->SetSessionID(sessionID);

			BinaryWriter<Message> bw(msg);
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

	NetworkComponent::NetworkComponent()
	{
	}

	NetworkComponent::~NetworkComponent()
	{
	}

	void NetworkComponent::InitNet(int netThreadNum)
	{
		m_NetworkComponentImp = std::make_shared<NetworkComponentImp>(netThreadNum);
	}

	bool NetworkComponent::Listen(const std::string& ip, const std::string& port)
	{
		Assert(ip.size() != 0 && port.size() != 0, "NetworkComponent::Listen: ip  and port nust not be null");
		Assert(nullptr != m_NetworkComponentImp, "NetworkComponent::Listen: Network not init");
		m_NetworkComponentImp->Net->Listen(ip, port);
		return (m_NetworkComponentImp->Net->GetErrorCode() == 0);
	}

	void NetworkComponent::Connect(const std::string& ip, const std::string& port)
	{
		Assert(nullptr != m_NetworkComponentImp, "NetworkComponent::Connect: Network not init");
		m_NetworkComponentImp->Net->AsyncConnect(ip, port);
	}

	SessionID NetworkComponent::SyncConnect(const std::string & ip, const std::string & port)
	{
		Assert(nullptr != m_NetworkComponentImp, "NetworkComponent::SyncConnect: Network not init");
		return m_NetworkComponentImp->Net->SyncConnect(ip, port);
	}

	void NetworkComponent::SendNetMessage(SessionID sessionID, Message* msg)
	{
		Assert(nullptr != m_NetworkComponentImp, "NetworkComponent::SendNetMessage: Network not init");
		m_NetworkComponentImp->Net->Send(sessionID, msg->ToStreamPointer());
		SAFE_DELETE(msg);
	}

	void NetworkComponent::Close(SessionID sessionID)
	{
		Assert(nullptr != m_NetworkComponentImp, "NetworkComponent::Close: Network not init");

		m_NetworkComponentImp->Net->CloseSession(sessionID, ESocketState::ForceClose);
	}

	void NetworkComponent::OnEnter()
	{
		if (nullptr == m_NetworkComponentImp)
			return;

		m_NetworkComponentImp->Net->Run();
	}

	void NetworkComponent::Update(uint32_t interval)
	{
		if (nullptr == m_NetworkComponentImp)
			return;

		if (OnNetMessage != nullptr)
		{
			auto msgs = m_NetworkComponentImp->NetMsgQueue.Move();
			for (auto& it : msgs)
			{
				OnNetMessage(it);
			}
		}
	}

	void NetworkComponent::OnExit()
	{
		if (nullptr == m_NetworkComponentImp)
			return;
		m_NetworkComponentImp->NetMsgQueue.Exit();
		m_NetworkComponentImp->Net->Stop();
	}

}

