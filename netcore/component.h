/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include "macro_define.h"

namespace moon
{
	DECLARE_SHARED_PTR(component)

	class MOON_EXPORT component
	{
	public:
		component() noexcept;

		component(const component&) = delete;

		component& operator=(const component&) = delete;

		virtual ~component();

		const std::string& name();

		template<typename TOwner>
		TOwner* owner()
		{
			return dynamic_cast<TOwner*>(owner_imp());
		}

		component* owner_imp();

		void set_enable_update(bool v);

		bool enable_update();

		component_ptr_t get_component_imp(const std::string& name);

		template<class TComponent, typename TComponentPtr = std::shared_ptr<TComponent> >
		TComponentPtr get_component(const std::string& name)
		{
			auto v = get_component_imp(name);
			if (v)
			{
				return std::dynamic_pointer_cast<TComponent>(v);
			}
			return nullptr;
		}

		bool add_component_imp(component_ptr_t v);

		template<class TComponent, typename TComponentPtr = std::shared_ptr<TComponent> >
		TComponentPtr add_component(const std::string& name)
		{
			static_assert(std::is_base_of<component, TComponent>::value, "TComponent must be child of component");
			do
			{
				auto t = std::make_shared<TComponent>();
				BREAK_IF(t == nullptr);
				t->set_name(name);
				t->set_owner(this);
				BREAK_IF(!add_component_imp(t));
				return t;
			} while (0);
			return nullptr;
		}

		void traverse(std::function<void(component*)> cb);

	protected:
		void set_name(const std::string& name);

		void set_owner(component* v);

		//override
		virtual void start();

		virtual void destory();

		virtual void update();
	private:
		struct  component_imp;
		component_imp*  component_imp_;
	};
}

