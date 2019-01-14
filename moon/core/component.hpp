#pragma once
#include "config.h"

namespace moon
{
    class log;
    DECLARE_SHARED_PTR(component);

    class component :public std::enable_shared_from_this<component>
    {
    public:
        component() = default;

        component(const component&) = delete;

        component& operator=(const component&) = delete;

        virtual ~component() = default;

        const std::string & component::name() const
        {
            return name_;
        }

        void set_name(const std::string & name)
        {
            name_ = name;
        }

        template<typename TParent>
        TParent* parent() const
        {
            return dynamic_cast<TParent*>(owner_);
        }

        void set_parent(component * v)
        {
            owner_ = v;
        }

        template<class TComponent>
        std::shared_ptr<TComponent> get_component(const std::string& name) const
        {
            if (auto iter = components_.find(name); iter != components_.end())
            {
                return std::dynamic_pointer_cast<TComponent>(iter->second);
            }
            return nullptr;
        }

        template<class TComponent,  typename... Args>
        std::shared_ptr<TComponent> add_component(const std::string& name, std::string_view config = std::string_view{}, Args&&... args)
        {
            component_ptr_t v = std::make_shared<TComponent>(std::forward<Args>(args)...);
            v->set_name(name);
            v->set_parent(this);
            v->logger(logger());
            v->ok(v->init(config));
            if (!v->ok())
            {
                return nullptr;
            }
            auto iter = components_.emplace(name, std::move(v));
            MOON_ASSERT(iter.second, "The component is already exist!");
            return std::dynamic_pointer_cast<TComponent>(iter.first->second);
        }

        bool remove(const std::string& name)
        {
            auto iter = components_.find(name);
            if (iter != components_.end())
            {
                iter->second->destroy();
                return true;
            }
            return false;
        }

        void for_each(std::function<void(component*)> callback)
        {
            for (auto& it : components_)
            {
                callback(it.second.get());
            }
        }

        log* logger() const
        {
            return log_;
        }

        void logger(log * l)
        {
            log_ = l;
        }

        bool started() const
        {
            return start_;
        }

        bool ok() const
        {
            return ok_;
        }

        void ok(bool v)
        {
            ok_ = v;
        }

    public:
        virtual bool init(std::string_view) = 0;

        virtual void start()
        {
            if (start_)
            {
                return;
            }

            start_ = true;

            for (auto& it : components_)
            {
                it.second->start();
            }
        }

        virtual void destroy()
        {
            ok_ = false;
            for (auto& it : components_)
            {
                it.second->destroy();
            }
        }
    private:
        bool start_ = false;
        bool ok_ = true;
        component* owner_ = nullptr;
        log* log_ = nullptr;
        std::string     name_;
        std::unordered_map<std::string, component_ptr_t>    components_;
    };
}

