#pragma once
#include "config.h"
#include "component.h"

namespace moon
{
    class log;
    class server;
    class router;
    class worker;

    class service :public component
    {
    public:
        friend class router;

        service();

        virtual ~service();

        uint32_t id() const;

        log* logger() const override;

        void set_server_context(server* s, router* r, worker* w);

        server* get_server();

        router* get_router();

        worker* get_worker();

        bool unique() const;

        void handle_message(const message_ptr_t& msg);

        void handle_message(message_ptr_t&& msg);

        void removeself(bool crashed = false);
    public:
        virtual void exit();

        virtual void dispatch(message* msg) = 0;

        virtual void on_timer(uint32_t, bool) {};
    protected:
        void set_unique(bool v);

        void set_id(uint32_t v);
    protected:
        bool unique_;
        uint32_t id_;
        server* server_;
        router* router_;
        worker* worker_;
    };
}

