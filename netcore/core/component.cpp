/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#include "component.h"

namespace moon
{
	struct  component::component_imp
	{
		component_imp() noexcept
			:owner_(nullptr)
			,enable_update_(false)
		{

		}

		std::string	 name_;
		component* owner_;
		bool enable_update_;
		std::unordered_map<std::string, component_ptr_t>	components_;
	};

	component::component() noexcept
		:component_imp_(new component_imp)
	{
	}

	component::~component()
	{
		SAFE_DELETE(component_imp_);
	}
	
	const std::string & component::name()
	{
		return component_imp_->name_;
	}

	component* component::owner_imp()
	{
		return component_imp_->owner_;
	}

	void component::set_enable_update(bool v)
	{
		component_imp_->enable_update_ = v;
	}

	bool component::enable_update()
	{
		return component_imp_->enable_update_;
	}

	component_ptr_t component::get_component_imp(const std::string & name)
	{
		auto iter = component_imp_->components_.find(name);
		if (iter != component_imp_->components_.end())
		{
			return iter->second;
		}
		return nullptr;
	}

	bool component::add_component_imp(component_ptr_t v)
	{
		auto iter = component_imp_->components_.emplace(v->name(), v);
		assert(iter.second&&"The component is already exist!");
		return iter.second;
	}

	void component::traverse(std::function<void(component*)> cb)
	{
		for (auto& it : component_imp_->components_)
		{
			cb(it.second.get());
		}
	}

	void component::set_name(const std::string & name)
	{
		component_imp_->name_ = name;
	}

	void component::set_owner(component * v)
	{
		component_imp_->owner_ = v;
	}

	void component::start()
	{
		for (auto& it : component_imp_->components_)
		{
			it.second->start();
		}
	}

	void component::destory()
	{
		for (auto& it : component_imp_->components_)
		{
			it.second->destory();
		}
	}

	void component::update()
	{
		for (auto& it : component_imp_->components_)
		{
			if (it.second->enable_update())
			{
				it.second->update();
			}
		}
	}

}

