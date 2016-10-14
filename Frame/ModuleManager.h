/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include "MacroDefine.h"
#include "Common/noncopyable.hpp"

namespace moon
{
	DECLARE_SHARED_PTR(Worker)
	DECLARE_SHARED_PTR(Module)
	DECLARE_SHARED_PTR(MemoryStream)

	//Module 管理类，负责Module的创建，调度，移除
	class ModuleManager:public noncopyable
	{
	public:
		ModuleManager();
		~ModuleManager();

		/**
		* 初始化
		* @config 初始化字符串 key-value形式 : machine_id:1;worker_num:2;
		*	machine_id 默认值是0， worker_num（工作者线程数目） 默认值是 1
		*/
		void			Init(const std::string& config);

		/**
		* 为了跨进程 Module通讯 设置MachineID,用于区分不同machine,最多可以有255个
		* MachineID 会保存在 ModuleID的高8位
		*
		* @return 初始化时所配置的machine_id,或者 0
		*/
		uint8_t		GetMachineID();

		/**
		* 创建Module
		*
		* @config 创建Module所需的配置，会传递给Module::Init
		*/
		template<typename TModule>
		void			CreateModule(const std::string& config);

		/**
		* 根据ID移除Module
		*
		* @moduleID 
		*/
		void			RemoveModule(ModuleID moduleID);

		/**
		* 向某个Module发送消息
		*
		* @sender 发送者id
		* @receiver 接收者id
		* @msg 消息内容
		*/
		void			Send(ModuleID sender, ModuleID receiver,const std::string& data, const std::string& userdata, uint64_t rpcID,uint8_t type);

		void			SendEx(ModuleID sender, ModuleID receiver, const MemoryStreamPtr& data,const std::string& userdata,uint64_t rpcID, uint8_t type);

		/**
		* 向所有Module（除了发送者）广播消息
		*
		* @sender	发送者id
		* @msg		消息内容
		*/
		void			Broadcast(ModuleID sender, const std::string& data,const std::string& userdata,uint8_t type);

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
		* @m
		*/
		void			AddModuleToWorker(uint8_t workerid,const ModulePtr& m);

		/**
		* 根据Module ID 获取所在的 workerID
		* @module id
		*/
		uint8_t		GetWorkerID(ModuleID id);

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


