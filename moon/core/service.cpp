#include "service.h"
#include "message.hpp"
#include "router.h"
#include "common/log.hpp"

namespace moon
{
    service::service()
        :unique_(false)
        , id_(0)
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
        return sctx_.this_logger;
    }

    const server_context& service::get_server_context() const
    {
        return sctx_;
    }

    void service::set_server_context(server_context sctx)
    {
        sctx_ = sctx;
    }

    void service::removeself(bool crashed)
    {
        sctx_.this_router->remove_service(id_, 0, 0, crashed);
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
                    sctx_.this_router->send_message(std::forward<message_ptr_t>(msg));
                }
            }
        }
        catch (std::exception& e)
        {
            CONSOLE_ERROR(logger(), "service::handle_message exception: %s", e.what());
        }
    }
}

