/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#include "Module.h"
#include "Message.h"
#include "ModuleManager.h"

namespace moon
{
	struct Module::ModuleImp
	{
		ModuleImp()
			:EnableUpdate(false)
			,Manager(nullptr)
			,Ok(true)
		{
		}

		ModuleID															ID;
		std::string																Name;
		std::string																Config;
		bool																		EnableUpdate;
		bool																		Ok;
		ModuleManager*												Manager;
		std::deque<MessagePtr>									MessageQueue;
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

	void Module::Broadcast(Message* msg)
	{
		m_ModuleImp->Manager->Broadcast(GetID(), msg);
	}

	void Module::Send(ModuleID receiver, Message* msg)
	{
		//if send Message to self , add to MessageQueue directly.
		if (receiver == GetID())
		{
			msg->SetReceiver(GetID());
			PushMessage(MessagePtr(msg));
			return;
		}
		m_ModuleImp->Manager->Send(GetID(),receiver, msg);
	}

	void Module::SetManager(ModuleManager* mgr)
	{
		m_ModuleImp->Manager = mgr;
	}

	void Module::Exit()
	{
		m_ModuleImp->Manager->RemoveModule(GetID());
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
		OnMessage(msg.get());
		mq.pop_front();
		return mq.size() != 0;
	}

	size_t Module::GetMQSize()
	{
		return m_ModuleImp->MessageQueue.size();
	}
}


