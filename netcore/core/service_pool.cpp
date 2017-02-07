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

	service_pool* service_pool::instance_ = nullptr;

	struct service_pool::service_pool_imp
	{
		uint8_t machineid_ = 0;
		bool run_ = false;
		std::vector<service_worker_ptr_t> workers_;
		std::unordered_map<std::string, register_func > regservices_;

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

	service_pool* service_pool::instance()
	{
		assert(nullptr != instance_);
		return instance_;
	}

	service_pool::service_pool()
		:imp_(nullptr)
	{
		imp_ = new service_pool_imp;

		assert(nullptr == instance_);
		instance_ = this;
	}

	service_pool::~service_pool()
	{
		stop();
		imp_->regservices_.clear();
		SAFE_DELETE(imp_);
		instance_ = nullptr;
	}

	void service_pool::init(uint8_t machineid, uint8_t worker_num)
	{
		imp_->machineid_ = machineid;

		for (uint8_t i = 0; i != worker_num; i++)
		{
			imp_->workers_.emplace_back(std::make_shared<service_worker>());
			auto& w = imp_->workers_.back();
			w->workerid(i);
			w->machineid(imp_->machineid_);
			w->set_service_pool(this);
		}

		CONSOLE_TRACE("service_pool initialized with %d worker thread.", worker_num);
	}

	uint8_t service_pool::machineid()
	{
		return imp_->machineid_;
	}

	void service_pool::run()
	{
		if (imp_->run_)
			return;

		if (imp_->workers_.size() == 0)
		{
			CONSOLE_TRACE("service_pool does not init");
			return;
		}

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

		for (auto iter = imp_->workers_.rbegin(); iter != imp_->workers_.rend(); iter++)
		{
			(*iter)->stop();
		}

		imp_->run_ = false;
		CONSOLE_TRACE("service_pool stop");
	}

	void service_pool::new_service(const std::string& type, const std::string& config, bool exclusived)
	{
		auto iter = imp_->regservices_.find(type);
		if (iter == imp_->regservices_.end())
		{
			CONSOLE_ERROR("new service failed:service type[%s] was not registered",type.data());
			return;
		}

		auto s = iter->second();

		auto worker = imp_->next_worker();
	
		worker->add_service(s, config, exclusived);
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
		if (!is_service_message(msg->type()) && msg->type()!=message_type::message_system)
		{
			CONSOLE_WARN("sending  illegal type message. can only send service type message or system message");
			return;
		}

		if (msg->receiver() == 0)
		{
			CONSOLE_WARN("message receiver id is 0");
			return;
		}

		uint8_t id = imp_->worker_id(msg->receiver());
		if (id < imp_->workers_.size())
		{
			imp_->workers_[id]->send(msg);
		}
	}

	void service_pool::broadcast(uint32_t sender, const message_ptr_t & msg)
	{
		if (msg->type() == message_type::unknown)
		{
			CONSOLE_WARN("broadcast unknown type message.");
			return;
		}

		msg->set_sender(sender);
		msg->set_broadcast(true);
		for (auto& w : imp_->workers_)
		{
			w->send(msg);
		}
	}

	bool service_pool::register_service(const std::string & type, register_func f)
	{
		auto ret = imp_->regservices_.emplace(type, f);
		return ret.second;
	}

	void service_pool::report_service_num(uint32_t subscriber, const std::string & subject)
	{
		for (auto iter = imp_->workers_.rbegin(); iter != imp_->workers_.rend(); iter++)
		{
			(*iter)->report_service_num(subscriber, subject);
		}
	}

	void service_pool::report_work_time(uint32_t subscriber, const std::string & subject)
	{
		for (auto iter = imp_->workers_.rbegin(); iter != imp_->workers_.rend(); iter++)
		{
			(*iter)->report_work_time(subscriber, subject);
		}
	}
}


