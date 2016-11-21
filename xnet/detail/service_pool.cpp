/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#include "service_pool.h"
#include "common/string.hpp"
#include "log.h"
#include "service_worker.h"
#include "message.h"

namespace moon
{
	DECLARE_SHARED_PTR(service_worker)

	struct service_pool::service_pool_imp
	{
		uint8_t machineid_ = 0;
		bool run_ = false;
		std::vector<service_worker_ptr_t> workers_;

		uint8_t next_worker_id()
		{
			static std::atomic<uint8_t> next_workerid_(0);
			if (next_workerid_ >= workers_.size())
			{
				next_workerid_ = 0;
			}
			return next_workerid_.fetch_add(1);
		}

		service_worker_ptr_t next_worker()
		{
			for (uint8_t i = 0; i < workers_.size(); i++)
			{
				uint8_t  id = next_worker_id();
				auto& w = workers_[id];
				if (w->exclusive())
					continue;
				return w;
			}

			for (uint8_t i = 0; i < workers_.size(); i++)
			{
				uint8_t  id = next_worker_id();
				return workers_[id];
			}

			return nullptr;
		}

		uint8_t worker_id(uint32_t serviceid)
		{
			return ((serviceid >> WORKER_ID_SHIFT) & 0xFF);
		}
	};

	service_pool::service_pool()
		:imp_(nullptr)
	{
		imp_ = new service_pool_imp;
	}

	service_pool::~service_pool()
	{
		SAFE_DELETE(imp_);
	}

	void service_pool::init(const std::string & config)
	{
		auto kv_config = parse_key_value_string(config);

		int service_worker_num = 1;
		imp_->machineid_ = 0;

		if (!contains_key(kv_config, "service_worker_num"))
		{
			CONSOLE_WARN("config does not have [service_worker_num],will initialize service_worker_num = 1");
		}
		else
		{
			service_worker_num = string_convert<int>(kv_config["service_worker_num"]);
		}

		if (!contains_key(kv_config, "machineid"))
		{
			CONSOLE_WARN("config does not have [machineid],will initialize machineid = 0");
		}
		else
		{
			imp_->machineid_ = string_convert<int>(kv_config["machineid"]);
		}

		for (uint8_t i = 0; i != service_worker_num; i++)
		{
			imp_->workers_.emplace_back(std::make_shared<service_worker>());
			auto& w = imp_->workers_.back();
			w->workerid(i);
			w->machineid(imp_->machineid_);
			w->set_service_pool(this);
		}

		CONSOLE_TRACE("service_pool initialized with %d worker thread.", service_worker_num);
	}

	uint8_t service_pool::machineid()
	{
		return imp_->machineid_;
	}

	void service_pool::run()
	{
		if (imp_->run_)
			return;

		CONSOLE_TRACE("service_pool start");
		for (auto& w : imp_->workers_)
		{
			w->run();
		}
		imp_->run_ = true;
	}

	void service_pool::stop()
	{
		if (!imp_->run_)
			return;

		for (auto& w : imp_->workers_)
		{
			w->stop();
		}
		CONSOLE_TRACE("service_pool stop");
	}

	void service_pool::new_service(const service_ptr_t& s, const std::string & config, bool exclusived)
	{
		auto worker = imp_->next_worker();
		worker->new_service(s, config, exclusived);
	}

	void service_pool::remove_service(uint32_t serviceid)
	{
		auto workerid = imp_->worker_id(serviceid);
		if (workerid < imp_->workers_.size())
		{
			imp_->workers_[workerid]->remove_service(serviceid);
		}
	}

	void service_pool::send(const message_ptr_t & msg)
	{
		Assert((msg->type() != MTYPE_UNKNOWN), "send unknown type message!");
		Assert((msg->receiver() != 0), "receiver id is 0!");
		uint8_t id = imp_->worker_id(msg->receiver());
		if (id < imp_->workers_.size())
		{
			imp_->workers_[id]->send(msg);
		}
	}

	void service_pool::broadcast(uint32_t sender, const std::string & data)
	{
		auto msg = std::make_shared<message>(data.size());
		msg->set_sender(sender);
		msg->set_receiver(0);
		msg->write_data(data);
		msg->set_type(MTYPE_BROADCAST);

		for (auto& w : imp_->workers_)
		{
			w->send(msg);
		}
	}
	void service_pool::broadcast_ex(uint32_t sender, const buffer_ptr_t & data)
	{
		auto msg = std::make_shared<message>(data);
		msg->set_sender(sender);
		msg->set_receiver(0);
		msg->set_type(MTYPE_BROADCAST);

		for (auto& w : imp_->workers_)
		{
			w->send(msg);
		}
	}
}


