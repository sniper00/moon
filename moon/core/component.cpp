#include "component.h"

namespace moon
{
    struct  component::component_imp
    {
        component_imp() noexcept
            :enable_update_(false)
            , start_(false)
            , ok_(true)
            , owner_(nullptr)
            , log_(nullptr)
        {

        }

        bool enable_update_;
        bool start_;
        bool ok_;
        component* owner_;
        log* log_;
        std::string     name_;
        std::unordered_map<std::string, component_ptr_t>    components_;
    };

    component::component() noexcept
        :component_imp_(nullptr)
    {
        component_imp_ = new(std::nothrow) component_imp;
    }

    component::~component()
    {
        SAFE_DELETE(component_imp_);
    }

    const std::string & component::name() const
    {
        return component_imp_->name_;
    }

    component* component::parent_imp() const
    {
        return component_imp_->owner_;
    }

    void component::set_enable_update(bool v)
    {
        component_imp_->enable_update_ = v;
    }

    bool component::enable_update() const
    {
        return component_imp_->enable_update_;
    }

    component_ptr_t component::get_component_imp(const std::string & name) const
    {
        auto iter = component_imp_->components_.find(name);
        if (iter != component_imp_->components_.end())
        {
            return iter->second;
        }
        return nullptr;
    }

    bool component::add_component_imp(const std::string& name, component_ptr_t v)
    {
        auto iter = component_imp_->components_.emplace(name, v);
        MOON_CHECK(iter.second, "The component is already exist!");
        if (iter.second)
        {
            v->set_name(name);
            v->set_parent(this);
            v->logger(logger());
            v->init();
            v->ok(true);
        }
        return iter.second;
    }

    bool component::remove(const std::string & name)
    {
        auto iter = component_imp_->components_.find(name);
        if (iter != component_imp_->components_.end())
        {
            iter->second->destroy();
            return true;
        }
        return false;
    }

    void component::for_all(std::function<void(component*)> cb)
    {
        for (auto& it : component_imp_->components_)
        {
            cb(it.second.get());
        }
    }

    log * component::logger() const
    {
        return component_imp_->log_;
    }

    void component::logger(log * l)
    {
        component_imp_->log_ = l;
    }

    bool component::started() const
    {
        return component_imp_->start_;
    }

    bool component::ok() const
    {
        return component_imp_->ok_;
    }

    void component::ok(bool v)
    {
        component_imp_->ok_ = v;
    }

    void component::set_name(const std::string & name)
    {
        component_imp_->name_ = name;
    }

    void component::set_parent(component * v)
    {
        component_imp_->owner_ = v;
    }

    void component::start()
    {
        if (component_imp_->start_)
        {
            return;
        }

        component_imp_->start_ = true;

        for (auto& it : component_imp_->components_)
        {
            it.second->start();
        }
    }

    void component::destroy()
    {
        component_imp_->ok_ = false;
        for (auto& it : component_imp_->components_)
        {
            it.second->destroy();
        }
    }
}

