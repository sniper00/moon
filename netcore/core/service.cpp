#include "service.h"
/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#include "service.h"
#include "message.h"
#include "service_pool.h"
#include "service_worker.h"
#include "log.h"
#include "common/string.hpp"
#include "macro_define.h"

namespace moon
{
	DECLARE_SHARED_PTR(network);

	struct service::service_imp
	{
		service_imp()
			:exit_(false)
			, serviceid_(0)
			, cache_uuid_(0)
		{
		}

		~service_imp()
		{
		}
		bool exit_;
		uint32_t serviceid_;
		uint32_t cache_uuid_;
		std::unordered_map<uint32_t, buffer_ptr_t> caches_;
	};

	service::service()
		:service_imp_(nullptr)
		, pool_(nullptr)
		, worker_(nullptr)
	{
		service_imp_ = new service_imp();
	}

	service::~service()
	{
		SAFE_DELETE(service_imp_);
	}

	uint32_t service::serviceid() const
	{
		return service_imp_->serviceid_;
	}

	void service::send(uint32_t receiver, const std::string& data, const std::string& userctx, uint32_t rpc, message_type type)
	{
		if (!is_service_message(type))
		{
			CONSOLE_WARN("sending  illegal type message. can only send service type message");
			return;
		}

		auto buf = create_buffer(data.size());
		buf->write_back(data.data(), 0, data.size());
		send_imp(receiver, buf, userctx, rpc, type);
	}

	void service::broadcast(const message_ptr_t& msg)
	{
		pool_->broadcast(serviceid(), msg);
	}

	void service::send_cache(uint32_t receiver, uint32_t cacheid, const std::string& userctx, uint32_t rpc, message_type type)
	{
		if (!is_service_message(type))
		{
			CONSOLE_WARN("sending  illegal type message. can only send service type message");
			return;
		}

		auto iter = service_imp_->caches_.find(cacheid);
		if (iter == service_imp_->caches_.end())
		{
			CONSOLE_WARN("send_cache failed, can not find cache data id %s", cacheid);
			return;
		}

		send_imp(receiver, iter->second, userctx, rpc, type);
	}


	uint32_t service::make_cache(const std::string & data)
	{
		auto ms = create_buffer(data.size());
		ms->write_back(data.data(), 0, data.size());
		service_imp_->cache_uuid_++;
		service_imp_->caches_.emplace(service_imp_->cache_uuid_, ms);
		return service_imp_->cache_uuid_;
	}

	void service::new_service(const std::string& type,const std::string & config)
	{
		pool_->new_service(type,config,false);
	}

	void service::remove_service(uint32_t id)
	{
		pool_->remove_service(id);
	}

	void service::update()
	{
		component::update();
		service_imp_->cache_uuid_ = 0;
		service_imp_->caches_.clear();
	}

	void service::set_serviceid(uint32_t v)
	{
		service_imp_->serviceid_ = v;
	}

	void service::set_service_worker(service_worker * w)
	{
		worker_ = w;
		pool_ = w->get_service_pool();
	}

	void service::exit(bool v)
	{
		service_imp_->exit_ = v;
		remove_service(service_imp_->serviceid_);
	}

	bool service::exit()
	{
		return service_imp_->exit_;
	}

	void service::send_imp(uint32_t receiver, const buffer_ptr_t& data, const std::string& userctx, uint32_t rpc, message_type type)
	{
		message_ptr_t msg = std::make_shared<message>(data);
		msg->set_sender(service_imp_->serviceid_);
		msg->set_receiver(receiver);
		msg->set_userctx(userctx);
		msg->set_type(type);
		msg->set_rpc(rpc);

		// if in the same thread, push to message queue directly. (locked queue)
		if ( (receiver&0x00FF0000U) == (service_imp_->serviceid_&0x00FF0000U))
		{
			worker_->push_message(msg);
			return;
		}
		pool_->send(msg);
	}

	void service::report_service_num(const std::string & subject)
	{
		pool_->report_service_num(service_imp_->serviceid_, subject);
	}

	void service::report_work_time(const std::string & subject)
	{
		pool_->report_work_time(service_imp_->serviceid_, subject);
	}

	void service::handle_message(const message_ptr_t& msg)
	{
		assert(serviceid() == msg->receiver() || msg->receiver() == 0);
		on_message(msg.get());
		if (msg->receiver() != serviceid() && msg->receiver() != 0)
		{
			//if forward a network message, set it's type service_send
			if (msg->type() == message_type::network_recv)
			{
				msg->set_type(message_type::service_send);
			}
			pool_->send(msg);
		}
	}
}

