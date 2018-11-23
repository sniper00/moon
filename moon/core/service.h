#pragma once
#include "config.h"
#include "component.h"

namespace moon
{
    class log;
    class router;

    class service :public component
    {
    public:
        friend class router;

        service();

        virtual ~service();

        uint32_t id() const;

        log* logger() const override;

        router* get_router() const;

        void set_router(router* r);

        bool unique() const;

        void handle_message(const message_ptr_t& msg);

        void handle_message(message_ptr_t&& msg);

        void removeself(bool crashed = false);
    public:
        virtual void exit();

        virtual bool init(const string_view_t& config) = 0;

        virtual void dispatch(message* msg) = 0;

        virtual void runcmd(uint32_t sender, const std::string& cmd, int32_t responseid);
    protected:
        void set_unique(bool v);

        void set_id(uint32_t v);
    private:
        bool unique_;
        uint32_t id_;
        router* router_;
    };
}

