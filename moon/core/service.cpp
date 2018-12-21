#include "service.h"
#include "message.hpp"
#include "router.h"
#include "common/log.hpp"

namespace moon
{
    service::service()
        :unique_(false)
        , id_(0)
        , server_(nullptr)
        , router_(nullptr)
        , worker_(nullptr)
    {

    }

    service::~service()
    {

    }

    bool service::unique() const
    {
        return unique_;
    }

    uint32_t service::id() const
    {
        return id_;
    }

    void service::set_id(uint32_t v)
    {
        id_ = v;
    }

    void service::exit()
    {
        removeself();
    }

    log * service::logger() const
    {
        return router_->logger();
    }

    void service::set_server_context(server * s, router * r, worker * w)
    {
        server_ = s;
        router_ = r;
        worker_ = w;
    }

    server * service::get_server()
    {
        return server_;
    }

    router * service::get_router()
    {
        return router_;
    }

    worker * service::get_worker()
    {
        return worker_;
    }

    void service::removeself(bool crashed)
    {
        router_->remove_service(id_, 0, 0, crashed);
    }

    void service::set_unique(bool v)
    {
        unique_ = v;
    }

    void service::handle_message(const message_ptr_t& msg)
    {
        try
        {
            dispatch(msg.get());
            if (msg->receiver() != id() && msg->receiver() != 0)
            {
                MOON_CHECK(false, "can not redirect this message");
            }
        }
        catch (std::exception& e)
        {
            CONSOLE_ERROR(logger(), "service::handle_message exception: %s", e.what());
        }
    }

    void service::handle_message(message_ptr_t && msg)
    {
        try
        {
            dispatch(msg.get());
            //redirect message
            if (msg->receiver() != id() && msg->receiver() != 0)
            {
                bool b = msg->broadcast();
                MOON_DCHECK(!b, "can not redirect broadcast message");
                if (!b)
                {
                    router_->send_message(std::forward<message_ptr_t>(msg));
                }
            }
        }
        catch (std::exception& e)
        {
            CONSOLE_ERROR(logger(), "service::handle_message exception: %s", e.what());
        }
    }
}

