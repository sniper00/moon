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

        const server_context& get_server_context() const;

        void set_server_context(server_context sctx);

        bool unique() const;

        void handle_message(const message_ptr_t& msg);

        void handle_message(message_ptr_t&& msg);

        void removeself(bool crashed = false);
    public:
        virtual void exit();

        virtual bool init(const string_view_t& config) = 0;

        virtual void dispatch(message* msg) = 0;
    protected:
        void set_unique(bool v);

        void set_id(uint32_t v);
    protected:
        bool unique_;
        uint32_t id_;
        server_context sctx_;
    };
}

