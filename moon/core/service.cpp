#include "service.h"
/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#include "service.h"
#include "message.hpp"
#include "router.h"
#include "common/log.hpp"

namespace moon
{
    service::service()
		:unique_(false)
		, id_(0)
		, router_(nullptr)
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

	router * service::get_router() const
	{
		return router_;
	}

	void service::set_router(router * r)
	{
		router_ = r;
	}

    void service::removeself(bool crashed)
    {
		router_->remove_service(id_, 0, 0, crashed);
    }

	void service::runcmd(uint32_t sender, const std::string & cmd, int32_t responseid)
	{
		CONSOLE_WARN(logger(), "service::runcmd [%s] not Implemented: sender[%u] responseid[%d]",cmd.data(), sender, responseid);
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
            //redirect message
            if (msg->receiver() != id() && msg->receiver() != 0)
            {
				bool b = msg->broadcast();
                MOON_DCHECK(!b, "can not redirect broadcast message");
				if (!b)
				{
					router_->send_message(msg);
				}
            }
        }
        catch (std::exception& e)
        {
            CONSOLE_ERROR(logger(), "service::handle_message exception: %s", e.what());
        }
    }
}

