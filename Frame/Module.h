/****************************************************************************

Git <https://github.com/sniper00/moon_net>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include "MacroDefine.h"
#include "Common/noncopyable.hpp"

namespace moon
{
	class ModuleManager;
	DECLARE_SHARED_PTR(Message);

	class  Module:public noncopyable
	{
	public:
		friend class Worker;
		friend class ModuleManager;

		Module() noexcept;
		virtual ~Module() {}

		ModuleID					GetID() const;
		const std::string			GetName() const;

		void								SetEnableUpdate(bool);
		bool								IsEnableUpdate();

		void								SendMessage(ModuleID receiver, const MessagePtr& msg);

		void								BroadcastMessage(const MessagePtr& msg);

		void								PushMessage(const MessagePtr& msg);

	protected:
		virtual bool					Init(const std::string& config) { return true; };

		virtual void					OnEnter() {}

		virtual void					Update(uint32_t interval) {}

		virtual void					OnExit() {}

		virtual void					OnMessage(const MessagePtr&) {}
	protected:
		size_t							GetMQSize();
		void								SetID(ModuleID moduleID);
		void								SetName(const std::string& name);
		void								SetManager(ModuleManager* mgr);
		void								Exit();
		void								SetOK(bool v);
		bool								IsOk();
		/**
		*	check Message queue,handle a Message,if Message queue size >0,return true else return false
		*	@return 
		*/
		bool								PeekMessage();
	private:
		struct  ModuleImp;
		std::shared_ptr<ModuleImp>		m_ModuleImp;
	};
};

