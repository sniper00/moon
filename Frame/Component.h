/****************************************************************************

Git <https://github.com/sniper00/moon_net>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include "Common/noncopyable.hpp"
#include "MacroDefine.h"

namespace moon
{
	DECLARE_SHARED_PTR(Component)
	DECLARE_WEAK_PTR(Component)

	DECLARE_SHARED_PTR(Module)
	DECLARE_WEAK_PTR(Module)

	class Component 
		:public std::enable_shared_from_this<Component>,noncopyable
	{
	public:
		Component() noexcept
			:m_EnableUpdate(false), m_Module(nullptr)
		{
		}

		const std::string& GetName() { return m_Name; }

		ComponentPtr GetOwner() { return m_Owner.lock(); }
		
		Module* GetModule() { return  (dynamic_cast<Module*>(m_Module)); }
		

		template<class TComponent, typename TComponentPtr = std::shared_ptr<TComponent> >
		TComponentPtr GetComponent(const std::string& name)
		{
			auto iter = m_Components.find(name);
			if (iter != m_Components.end())
			{
				return std::dynamic_pointer_cast<TComponent>(iter->second);
			}
			return nullptr;
		}

		template<class TComponent, typename TComponentPtr = std::shared_ptr<TComponent> >
		TComponentPtr AddComponent(const std::string& name)
		{
			do
			{
				auto t = std::make_shared<TComponent>();
				BREAK_IF(t == nullptr);
				t->SetName(name);
				t->SetModule(m_Module);
				t->SetOwner(shared_from_this());
				BREAK_IF(!t->Init());
				auto iter = m_Components.emplace(name, t);
				assert(iter.second&&"The component is already exist!");
				BREAK_IF(!iter.second);
				return t;
			} while (0);
			return nullptr;
		}

		template<class T>
		void SendEvent(T EventType, void* EventData)
		{
			for (auto& it : m_Components)
			{
				it.second->OnEvent(static_cast<int>(EventType), EventData);
			}
		}

		PROPERTY_READWRITE(bool, m_EnableUpdate, EnableUpdate)
	protected:
		void SetName(const std::string& name) { m_Name = name; }
		void SetOwner(const ComponentPtr& owner) { m_Owner = owner; }
		void SetModule(Module* module) { m_Module = module; }
		//override
	public:
		virtual bool Init()
		{
			for (auto& it : m_Components)
			{
				if (!it.second->Init())
				{
					return false;
				}
			}
			return true;
		}

		virtual void OnEnter()
		{
			for (auto& it : m_Components)
			{
				it.second->OnEnter();
			}
		}

		virtual void OnExit()
		{
			for (auto& it : m_Components)
			{			
				it.second->OnExit();
			}
		}

		virtual void Update(uint32_t interval)
		{
			for (auto& it : m_Components)
			{
				if (it.second->GetEnableUpdate())
				{
					it.second->Update(interval);
				}
			}
		}

		virtual void OnEvent(int eventType, void* EventData)
		{
			for (auto& it : m_Components)
			{
				it.second->OnEvent(eventType, EventData);
			}
		}
	protected:
		std::string																		m_Name;
		ComponentWPtr															m_Owner;
		Module*																		m_Module;
		std::unordered_map<std::string, ComponentPtr>		m_Components;
	};
}

