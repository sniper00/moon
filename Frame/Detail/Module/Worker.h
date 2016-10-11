/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include "MacroDefine.h"
#include "Common/LoopThread.hpp"
#include "Common/AsyncEvent.hpp"
#include "Common/noncopyable.hpp"

namespace moon
{
	class Message;

	DECLARE_SHARED_PTR(Module)
	DECLARE_SHARED_PTR(Message)
	//工作线程
	class Worker:public LoopThread,public AsyncEvent, noncopyable
	{
	public:
		Worker();

		~Worker();

		/**
		* 启动工作线程
		*
		*/
		void Run();

		/**
		* 停止工作线程
		*
		*/
		void Stop();

		/**
		* 向工作线程添加Module
		*
		*/
		void AddModule(const ModulePtr& m);

		/**
		* 移除Module
		*
		*/
		void RemoveModule(ModuleID id);

		/**
		* 消息分发
		*/
		void DispatchMessage(const MessagePtr& msg);

		/**
		* 向该Worker中的所有Module广播消息
		*/
		void Broadcast(const MessagePtr& msg);

		int	GetMessageFps();

		uint8_t		GetID();

		void			SetID(uint8_t id);
	private:
		void			Update(uint32_t interval);
	private:
		uint8_t																			m_WorkerID;
		std::unordered_map<ModuleID, ModulePtr>				m_Modules;
		//模块处理队列
		std::deque<Module*>													m_HandleQueue;

		std::atomic<int>															_fps;
		uint32_t																		_msg_counter;
		uint32_t																		_timer;
	};
};


