#include "service.h"
/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#include "service.h"
#include "message.hpp"
#include "server.h"
#include "worker.h"
#include "log.h"

namespace moon
{
    struct service::service_imp
    {
        service_imp()
            :unique_(false)
            , id_(0)
            , pool_(nullptr)
            , worker_(nullptr)
        {
        }

        ~service_imp()
        {
        }

        bool unique_;
        uint32_t id_;
        server* pool_;
        worker* worker_;
    };

    service::service()
        :service_imp_(new service_imp())
    {

    }

    service::~service()
    {
        SAFE_DELETE(service_imp_);
    }

    bool service::unique() const
    {
        return service_imp_->unique_;
    }

    uint32_t service::id() const
    {
        return service_imp_->id_;
    }

    uint32_t service::make_cache(const buffer_ptr_t & buf)
    {
        return get_worker()->make_cache(buf);
    }

    void service::send_cache(uint32_t receiver, uint32_t cacheid, const string_view_t& header, int32_t responseid, uint8_t type) const
    {
        const auto& buff = get_worker()->get_cache(cacheid);
        if (nullptr != buff)
        {
            get_server()->send(service_imp_->id_, receiver, buff, header, responseid, type);
        }
    }

    void service::set_id(uint32_t v)
    {
        service_imp_->id_ = v;
    }

    void service::set_worker(worker * w)
    {
        service_imp_->worker_ = w;
        service_imp_->pool_ = w->get_server();
    }

    void service::exit()
    {
        removeself();
    }

    log * service::logger() const
    {
        return get_server()->logger();
    }

    server * service::get_server() const
    {
        return service_imp_->pool_;
    }

    worker * service::get_worker() const
    {
        return service_imp_->worker_;
    }

    void service::removeself(bool crashed)
    {
        get_worker()->remove_service(service_imp_->id_, 0, 0, crashed);
    }

    void service::set_unique(bool v)
    {
        service_imp_->unique_ = v;
    }

    void service::handle_message(const message_ptr_t& msg)
    {
        try
        {
            if (msg->type() != PTYPE_SOCKET)
            {
                MOON_CHECK(id() == msg->receiver() || msg->receiver() == 0, "message receiver must be 0 or this serivice");
            }

            dispatch(msg.get());
            //redirect message
            if (msg->receiver() != id() && msg->receiver() != 0)
            {
                MOON_DCHECK(!msg->broadcast(), "can not redirect broadcast message");
                get_server()->send_message(msg);
            }
        }
        catch (std::exception& e)
        {
            CONSOLE_ERROR(logger(), "service::handle_message exception: %s", e.what());
        }
    }
}

