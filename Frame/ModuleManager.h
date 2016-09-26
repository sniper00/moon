/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include "MacroDefine.h"

namespace moon
{
	DECLARE_SHARED_PTR(Module)
	DECLARE_SHARED_PTR(Worker)
	class Message;

	//Module 管理类，负责Module的创建，调度，移除
	class ModuleManager
	{
	public:
		ModuleManager();
		~ModuleManager();

		/**
		* 初始化
		*
		* @workerNum 工作线程数目(至少为1)
		*/
		void			Init(const std::string& config);

		/**
		* 为了跨进程 Module通讯 设置MachineID,用于区分不同machine,最多可以有255个
		* MachineID 会保存在 ModuleID的高8位
		*
		* @return machine id
		*/
		uint8_t		GetMachineID();

		/**
		* 创建Module
		*
		* @config 创建Module所需的配置，会传递给Module::init
		*/
		template<typename TModule>
		void			CreateModule(const std::string& config);
		/**
		* 根据ID移除Module
		*
		* @id Module id
		*/
		void			RemoveModule(ModuleID moduleID);

		/**
		* 向某个Module发送消息
		*
		* @sender 发送者id
		* @receiver 接收者id
		* @msg 消息内容
		*/
		void			Send(ModuleID sender, ModuleID receiver,Message* msg);

		/**
		* 向所有Module（除了发送者）广播消息
		*
		* @sender 发送者id
		* @msg 消息内容
		*/
		void			Broadcast(ModuleID sender,Message* msg);

		/**
		* 启动所有Worker线程
		*
		*/
		void			Run();

		/**
		* 关闭所有Worker线程
		*
		*/
		void			Stop();

	private:
		/**
		* 轮询获取Worker ID
		*/
		uint8_t		GetNextWorkerID();

		/**
		* 把Module添加到 Worker
		* @workerid 
		* @actor_ptr 
		*/
		void			AddModuleToWorker(uint8_t workerid,const ModulePtr& act);

		/**
		* 根据Module ID 获取所在的 workerID
		* @actorID 
		*/
		uint8_t		GetWorkerID(ModuleID actorID);

	private:
		std::atomic<uint8_t>													m_nextWorker;
		std::atomic<uint16_t>													m_IncreaseModuleID;
		
		std::vector<WorkerPtr>												m_Workers;
		uint8_t																			m_MachineID;

		std::mutex																	m_ModuleNamesLock;
		std::vector<std::string>												m_ModuleNames;
	};

	template<typename TModule>
	void ModuleManager::CreateModule(const std::string& config)
	{
		uint16_t	incID = m_IncreaseModuleID.fetch_add(1);
		uint8_t		workerID = GetNextWorkerID();
		uint32_t	moduleID = 0;
		moduleID |= (uint32_t(m_MachineID) << 24);//Module ID 的 32-25 bit保存machineID
		moduleID |= (uint32_t(workerID) << 16);//Module ID 的 24-17 bit保存workerID
		moduleID |= (incID);

		auto module = std::make_shared<TModule>();
		module->SetID(moduleID);
		module->SetManager(this);

		if(module->Init(config))
		AddModuleToWorker(workerID, module);
	}
};


