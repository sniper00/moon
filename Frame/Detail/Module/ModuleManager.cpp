/****************************************************************************

Git <https://github.com/sniper00/moon_net>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#include "ModuleManager.h"
#include "Worker.h"
#include "Message.h"
#include "Module.h"
#include "Detail/Log/Log.h"
#include "Common/StringUtils.hpp"

namespace moon
{
	ModuleManager::ModuleManager()
		:m_nextWorker(0),  m_IncreaseModuleID(1), m_MachineID(1)
	{

	}

	ModuleManager::~ModuleManager()
	{

	}

	void ModuleManager::Init(const std::string& config)
	{
		auto kv_config = string_utils::parse_key_value_string(config);

		if (!contains_key(kv_config, "worker_num"))
		{
			CONSOLE_WARN("config does not have [worker_num],will initialize worker_num = 1");
		}

		if (!contains_key(kv_config, "machine_id"))
		{
			CONSOLE_WARN("config does not have [machine_id],will initialize machine_id = 1");
		}

		int workerNum;
	
		if (contains_key(kv_config, "worker_num"))
		{
			workerNum = string_utils::string_convert<int>(kv_config["worker_num"]);
		}
		else
		{
			workerNum = 1;
		}

		if (contains_key(kv_config, "machine_id"))
		{
			m_MachineID = string_utils::string_convert<int>(kv_config["machine_id"]);
		}
		else
		{
			m_MachineID = 0;
		}

		for (uint8_t i = 0; i != workerNum; i++)
		{
			auto wk = std::make_shared<Worker>();
			m_Workers.push_back(wk);
			wk->SetID(i);
		}

		CONSOLE_TRACE("ModuleManager initialized with %d Worker thread.", workerNum);
	}

	uint8_t ModuleManager::GetMachineID()
	{
		return m_MachineID;
	}

	void ModuleManager::RemoveModule(ModuleID actorID)
	{
		uint8_t workerID = GetWorkerID(actorID);
		if (workerID < m_Workers.size())
		{		
			m_Workers[workerID]->RemoveModule(actorID);
		}
	}

	void ModuleManager::SendMessage(ModuleID sender, ModuleID receiver,const MessagePtr& msg)
	{
		Assert((msg->GetType() != EMessageType::Unknown),"sending unknown type message");
		msg->SetSender(sender);
		msg->SetReceiver(receiver);
		uint8_t workerID = GetWorkerID(receiver);
		if (workerID < m_Workers.size())
		{
			m_Workers[workerID]->DispatchMessage(msg);
		}
	}

	void ModuleManager::BroadcastMessage(ModuleID sender,const MessagePtr& msg)
	{
		Assert((msg->GetType() != EMessageType::Unknown), "sending unknown type message");
		msg->SetSender(sender);
		msg->SetReceiver(0);
		for (auto& w : m_Workers)
		{
			w->BroadcastMessage(msg);
		}
	}

	void ModuleManager::Run()
	{
		CONSOLE_TRACE("ModuleManager start");
		for (auto& w : m_Workers)
		{
			w->Run();
		}
	}

	void ModuleManager::Stop()
	{
		for (auto& w : m_Workers)
		{
			w->Stop();
		}
		CONSOLE_TRACE("ModuleManager stop");
	}

	uint8_t ModuleManager::GetNextWorkerID()
	{
		if (m_nextWorker < m_Workers.size())
		{
			uint8_t tmp = m_nextWorker.load();
			m_nextWorker++;
			if (m_nextWorker == m_Workers.size())
			{
				m_nextWorker = 0;
			}
			return tmp;
		}
		return 0;
	}

	void ModuleManager::AddModuleToWorker(uint8_t workerid,const ModulePtr& module)
	{
		for (auto& wk : m_Workers)
		{
			if (wk->GetID() == workerid)
			{
				wk->AddModule(module);
				return;
			}
		}
	}

	uint8_t ModuleManager::GetWorkerID(ModuleID actorID)
	{
		return ((actorID >> 16) & 0xFF);
	}
};

