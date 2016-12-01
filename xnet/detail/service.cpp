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
#include "network.h"

namespace moon
{
	DECLARE_SHARED_PTR(network);

	struct service::service_imp
	{
		service_imp()
			:serviceid_(0)
			, enable_update_(true)
			, exit_(false)
			, pool_(nullptr)
			, worker_(nullptr)
			, cache_uuid_(0)
		{
		}

		~service_imp()
		{
		}

		uint32_t serviceid_;
		std::string	 name_;
		std::string	 config_;
		bool enable_update_;
		bool exit_;
		service_pool* pool_;
		service_worker* worker_;
		uint32_t cache_uuid_;
		network_ptr_t net_;
		std::unordered_map<std::string, std::string> kv_config_;
		std::unordered_map<uint32_t, buffer_ptr_t> caches_;
	};

	service::service()
		:service_imp_(nullptr)
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

	std::string service::name() const
	{
		return service_imp_->name_;
	}

	void service::set_enbale_update(bool v)
	{
		service_imp_->enable_update_ = v;
	}

	bool service::enable_update()
	{
		return service_imp_->enable_update_;
	}

	void service::send(uint32_t receiver, const std::string& data, const std::string& userctx, uint32_t rpc, message_type type)
	{
		if (!is_service_message(type))
		{
			CONSOLE_WARN("sending  illegal type message. can only send service type message");
			return;
		}

		auto ms = create_buffer(data.size());
		ms->write_back(data.data(), 0, data.size());
		send_imp(receiver, ms, userctx, rpc, type);
	}

	void service::broadcast(const std::string& data, message_type type)
	{
		service_imp_->pool_->broadcast(serviceid(), data,type);
	}

	void service::broadcast_ex(const message_ptr_t& msg)
	{
		service_imp_->pool_->broadcast_ex(serviceid(), msg);
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

	void service::new_service(const std::string & config)
	{
		auto s = clone();
		service_imp_->pool_->new_service(s,config,false);
	}

	void service::remove_service(uint32_t id)
	{
		service_imp_->pool_->remove_service(id);
	}

	void service::init_net(uint8_t threadnum, uint32_t timeout)
	{
		service_imp_->net_ = std::make_shared<network>(threadnum, timeout);
		service_imp_->net_->set_max_queue_size(2000);
		service_imp_->net_->set_network_handle([this](const message_ptr_t& m) {
			if (m->broadcast())
			{
				broadcast_ex(m);
			}
			else
			{
				handle_message(m);
			}
		});	
	}

	void service::net_send(uint32_t sessionid, const buffer_ptr_t & data)
	{
		if (nullptr != service_imp_->net_)
		{
			service_imp_->net_->send(sessionid, data);
		}
	}

	void service::listen(const std::string & ip, const std::string & port)
	{
		if (nullptr != service_imp_->net_)
		{
			service_imp_->net_->listen(ip, port);
		}
	}

	uint32_t service::sync_connect(const std::string & ip, const std::string & port)
	{
		if (nullptr != service_imp_->net_)
		{
			return service_imp_->net_->sync_connect(ip, port);
		}
		return 0;
	}

	void service::async_connect(const std::string & ip, const std::string & port)
	{
		if (nullptr != service_imp_->net_)
		{
			service_imp_->net_->async_connect(ip, port);
		}
	}

	bool service::check_config(const std::string & key)
	{
		return contains_key(service_imp_->kv_config_, key);
	}

	std::string service::get_config(const std::string & key)
	{
		if (contains_key(service_imp_->kv_config_, key))
		{
			return service_imp_->kv_config_[key];
		}
		return "";
	}

	bool service::init(const std::string & config)
	{
		service_imp_->config_ = config;
		service_imp_->kv_config_ = parse_key_value_string(config);

		return true;
	}

	void service::start()
	{
		if (nullptr != service_imp_->net_)
		{
			service_imp_->net_->run();
		}
	}

	void service::destory()
	{
		if (nullptr != service_imp_->net_)
		{
			service_imp_->net_->stop();
		}
	}

	void service::update()
	{
		service_imp_->cache_uuid_ = 0;
		service_imp_->caches_.clear();

		if (nullptr != service_imp_->net_)
		{
			service_imp_->net_->update();
		}
	}

	void service::set_serviceid(uint32_t v)
	{
		service_imp_->serviceid_ = v;
	}

	void service::set_name(const std::string & name)
	{
		service_imp_->name_ = name;
	}

	void service::set_service_pool(service_pool * pool)
	{
		service_imp_->pool_ = pool;
	}

	void service::set_service_worker(service_worker * w)
	{
		service_imp_->worker_ = w;
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

	void service::send_imp(uint32_t receiver, const buffer_ptr_t& ms, const std::string& userctx, uint32_t rpc, message_type type)
	{
		message_ptr_t msg = std::make_shared<message>(ms);
		msg->set_sender(service_imp_->serviceid_);
		msg->set_receiver(receiver);
		msg->set_userctx(userctx);
		msg->set_type(type);
		msg->set_rpc(rpc);

		// for the locked queue
		if ( (receiver&0x00FF0000) == (service_imp_->serviceid_&0x00FF0000))
		{
			service_imp_->worker_->push_message(msg);
			return;
		}
		service_imp_->pool_->send(msg);
	}

	void service::handle_message(const message_ptr_t& msg)
	{
		assert(serviceid() == msg->receiver() || msg->receiver() == 0);
		on_message(msg.get());
		if (msg->receiver() != serviceid() && msg->receiver() != 0)
		{
			service_imp_->pool_->send(msg);
		}
	}
}

