/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#include "service_worker.h"
#include "log.h"
#include "service.h"
#include "message.h"

namespace moon
{

	service_worker::service_worker()
		:workerid_(0)
		, machineid_(0)
		, serviceuid_(1)
		, work_time_(0)
		, exclusived_(false)
		, message_counter_(0)
		, pool_(nullptr)
	{
	}

	service_worker::~service_worker()
	{
	}

	void service_worker::run()
	{
		set_interval(20);
		on_update = std::bind(&service_worker::update, this, std::placeholders::_1);
		loop_thread::run();
		CONSOLE_TRACE("service_worker [%d] start", workerid_);
	}

	void service_worker::stop()
	{
		post([this]() {
			for (auto& iter : services_)
			{
				iter.second->destory();
				CONSOLE_TRACE("service [%s:%u] destory", iter.second->name().c_str(), iter.second->serviceid());
			}
			services_.clear();
		});

		async_event::exit();

		while (async_event::size() != 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		loop_thread::stop();

		CONSOLE_TRACE("service_worker [%d] Stop", workerid_);
	}

	uint32_t service_worker::make_serviceid()
	{
		if (serviceuid_ == std::numeric_limits<uint16_t>::max())
		{
			serviceuid_ = 1;
		}

		uint32_t tmp = serviceuid_++;
		uint8_t wkid = workerid();
		tmp |= uint32_t(machineid_) << MACHINE_ID_SHIFT;
		tmp |= uint32_t(wkid) << WORKER_ID_SHIFT;
		return tmp;
	}

	void service_worker::new_service(const service_ptr_t& s, const std::string& config, bool exclusived)
	{
		post([this, s, config, exclusived]() {
			uint32_t id = 0;
			while (1)
			{
				id = make_serviceid();
				if (services_.end() == services_.find(id))
					break;
			}
			exclusive(exclusived);
			s->set_serviceid(id);
			s->set_service_pool(pool_);
			s->init(config);
			s->set_service_worker(this);
			services_.emplace(s->serviceid(), s);
			CONSOLE_TRACE("service_worker[%d] : service start name:[%s] id[%u]", workerid_, s->name().c_str(), s->serviceid());
			s->start();
		});
	}

	void service_worker::remove_service(uint32_t id)
	{
		post([this, id]() {
			auto iter = services_.find(id);
			assert(iter != services_.end());
			iter->second->destory();
			CONSOLE_TRACE("service [%s:%u] destory", iter->second->name().c_str(), iter->second->serviceid());
			services_.erase(id);
		});
	}

	void service_worker::send(const message_ptr_t & msg)
	{
		post([this, msg]() {
			push_message(msg);
		});
	}

	int64_t service_worker::work_time()
	{
		return work_time_;
	}

	uint8_t service_worker::workerid()
	{
		return workerid_;
	}

	void service_worker::workerid(uint8_t id)
	{
		workerid_ = id;
	}

	void service_worker::machineid(uint8_t v)
	{
		machineid_ = v;
	}

	void service_worker::set_service_pool(service_pool * v)
	{
		pool_ = v;
	}

	void service_worker::exclusive(bool v)
	{
		exclusived_ = v;
	}

	bool service_worker::exclusive()
	{
		return exclusived_;
	}

	void service_worker::push_message(const message_ptr_t & msg)
	{
		message_queue_.push_back(msg);
	}

	service * service_worker::find(uint32_t serviceid)
	{
		auto iter = services_.find(serviceid);
		if (iter != services_.end())
		{
			return iter->second.get();
		}
		return nullptr;
	}

	void service_worker::update(uint32_t interval)
	{
		auto tb = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		update_event();
		service* s = nullptr;

		while (message_queue_.size() != 0)
		{
			auto msg = message_queue_.front();
			message_queue_.pop_front();

			if (msg->broadcast())
			{
				for (auto& s : services_)
				{
					if (s.second->serviceid() != msg->sender())
					{
						s.second->handle_message(msg);
					}
				}
				continue;
			}

			if (nullptr == s || s->serviceid() != msg->receiver())
			{
				s = find(msg->receiver());
				if (nullptr == s)
					continue;
			}
			s->handle_message(msg);
		}

		for (auto& s : services_)
		{
			if (s.second->enable_update())
			{
				s.second->update();
			}
		}

		auto te = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		work_time_.store(te - tb);
	}
}
