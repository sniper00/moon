/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#include "Module.h"
#include "Message.h"
#include "ModuleManager.h"
#include "ObjectCreateHelper.h"

namespace moon
{
	struct Module::ModuleImp
	{
		ModuleImp()
			:ID(0)
			,IncreCacheID(1)
			,EnableUpdate(false)
			,Manager(nullptr)
			,Ok(true)
		{
		}

		ModuleID																ID;
		uint32_t																IncreCacheID;
		std::string																Name;
		std::string																Config;
		bool																		EnableUpdate;
		bool																		Ok;
		ModuleManager*													Manager;
		std::deque<MessagePtr>									MessageQueue;
		std::unordered_map<uint32_t, MemoryStreamPtr> CacheDatas;
	};

	Module::Module() noexcept
		:m_ModuleImp(std::make_shared<Module::ModuleImp>())
	{
		SetEnableUpdate(true);
	}

	void Module::SetID(ModuleID moduleID)
	{
		m_ModuleImp->ID = moduleID;
	}

	void Module::SetName(const std::string& name)
	{
		m_ModuleImp->Name = name;
	}

	ModuleID Module::GetID() const
	{
		return m_ModuleImp->ID;
	}

	const std::string Module::GetName() const
	{
		return m_ModuleImp->Name;
	}

	void Module::SetEnableUpdate(bool v)
	{
		m_ModuleImp->EnableUpdate = v;
	}

	bool Module::IsEnableUpdate()
	{
		return m_ModuleImp->EnableUpdate;
	}

	void Module::Send(ModuleID receiver, const std::string & data, const std::string & userdata, uint64_t rpcID, uint8_t type)
	{
		//if send Message to self , add to MessageQueue directly.
		if (receiver == GetID())
		{
			Assert((type != (uint8_t)EMessageType::Unknown), "send unknown type message!");

			auto msg = ObjectCreateHelper<Message>::Create(data.size());
			msg->SetSender(GetID());
			msg->SetReceiver(GetID());
			msg->WriteData(data);
			msg->SetUserData(userdata);
			msg->SetRPCID(rpcID);
			msg->SetType(EMessageType(type));

			PushMessage(msg);
			return;
		}
		m_ModuleImp->Manager->Send(GetID(), receiver, data, userdata, rpcID, type);
	}

	void Module::SendByCache(ModuleID receiver, uint32_t cacheID, const std::string & userdata, uint64_t rpcID, uint8_t type)
	{
		auto iter = m_ModuleImp->CacheDatas.find(cacheID);
		if (iter == m_ModuleImp->CacheDatas.end())
			return;
		auto& ms = iter->second;
		//if send Message to self , add to MessageQueue directly.
		if (receiver == GetID())
		{
			Assert((type != (uint8_t)EMessageType::Unknown), "send unknown type message!");

			auto msg = ObjectCreateHelper<Message>::Create(ms);
			msg->SetSender(GetID());
			msg->SetReceiver(GetID());
			msg->SetUserData(userdata);
			msg->SetType(EMessageType(type));
			msg->SetRPCID(rpcID);

			PushMessage(msg);
			return;
		}
		m_ModuleImp->Manager->SendEx(GetID(), receiver,ms, userdata, rpcID, type);
	}

	void Module::Broadcast(const std::string& data, const std::string& userdata,uint8_t type)
	{
		m_ModuleImp->Manager->Broadcast(GetID(),data, userdata, type);
	}

	uint32_t Module::CreateCache(const std::string & data)
	{
		auto ms = ObjectCreateHelper<MemoryStream>::Create(data.size());
		auto cacheID = m_ModuleImp->IncreCacheID++;
		m_ModuleImp->CacheDatas.emplace(cacheID, ms);
		return cacheID;
	}

	void Module::SetManager(ModuleManager* mgr)
	{
		m_ModuleImp->Manager = mgr;
	}

	void Module::Exit()
	{
		m_ModuleImp->Manager->RemoveModule(GetID());
	}

	void Module::Update(uint32_t interval)
	{
		m_ModuleImp->IncreCacheID = 1;
		m_ModuleImp->CacheDatas.clear();
	}

	void Module::SetOK(bool v)
	{
		m_ModuleImp->Ok = v;
	}

	bool Module::IsOk()
	{
		return m_ModuleImp->Ok;
	}

	void Module::PushMessage(const MessagePtr& msg)
	{
		m_ModuleImp->MessageQueue.push_back(msg);
	}

	bool Module::PeekMessage()
	{
		auto& mq = m_ModuleImp->MessageQueue;

		if (mq.size() == 0)
			return false;
		auto& msg = mq.front();
		assert(GetID() == msg->GetReceiver() || msg->GetReceiver() == 0);
		OnMessage(msg->GetSender(),msg->Bytes(),msg->GetUserData(),msg->GetRPCID(),(uint8_t)msg->GetType());
		mq.pop_front();
		return mq.size() != 0;
	}

	size_t Module::GetMQSize()
	{
		return m_ModuleImp->MessageQueue.size();
	}
}


