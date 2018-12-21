#pragma once
#include "config.h"

namespace moon
{
    class log;
	DECLARE_SHARED_PTR(component);

    class component:public std::enable_shared_from_this<component>
    {
    public:
        component() noexcept;

        component(const component&) = delete;

        component& operator=(const component&) = delete;

        virtual ~component();

        void set_name(const std::string& name);
        const std::string& name() const;

        template<typename TParent>
        TParent* parent() const
        {
            return dynamic_cast<TParent*>(parent_imp());
        }

        void set_enable_update(bool v);

        bool enable_update() const;

        template<class TComponent, typename TComponentPtr = std::shared_ptr<TComponent> >
        TComponentPtr get_component(const std::string& name) const
        {
            auto v = get_component_imp(name);
            if (v!=nullptr)
            {
                return std::dynamic_pointer_cast<TComponent>(v);
            }
            return nullptr;
        }

        template<class TComponent, typename TComponentPtr = std::shared_ptr<TComponent> >
        TComponentPtr add_component(const std::string& name)
        {
            static_assert(std::is_base_of<component, TComponent>::value, "TComponent must be child of component");
            do
            {
                auto t = std::make_shared<TComponent>();
                BREAK_IF(t == nullptr);
                BREAK_IF(!add_component_imp(name,t));
                return t;
            } while (0);
            return nullptr;
        }

        bool remove(const std::string& name);

        void for_all(std::function<void(component*)> cb);

        virtual log* logger() const;
        void logger(log* l);

        bool started() const;

        bool ok() const;

        void ok(bool v);

        void set_parent(component* v);

        virtual void init() {}

        virtual void start();

        virtual void destroy();
	protected:
        component_ptr_t get_component_imp(const std::string& name) const;

        bool add_component_imp(const std::string& name,component_ptr_t v);

        component* parent_imp() const;
    private:
        struct  component_imp;
        component_imp*  component_imp_;
    };
}

