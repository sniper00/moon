#include "io_worker.h"
#include "session.h"
#include "log.h"
#include "message.h"
#include "common/string.hpp"

namespace moon
{
	io_worker::io_worker()
		:work_(ios_)
		, checker_(ios_)
		, workerid_(0)
		, sessionuid_(1)
		, sessionnum_(0)
	{}

	void io_worker::run()
	{
		start_checker();
		ios_.run();
	}

	void io_worker::stop()
	{
		ios_.post([this]() {
			close_all_sessions();
			checker_.cancel();
			ios_.stop();
		});
	}

	asio::io_service & io_worker::io_service()
	{
		return ios_;
	}

	uint8_t io_worker::workerid()
	{
		return workerid_;
	}

	void io_worker::workerid(uint8_t v)
	{
		workerid_ = v;
	}

	session_ptr_t io_worker::create_session(uint32_t time_out)
	{
		auto sessionid = make_sessionid();
		return std::make_shared<session>(*this, sessionid, time_out);
	}

	void io_worker::add_session(const session_ptr_t & s)
	{
		ios_.post([this, s]() {
			uint32_t sessionid = s->sessionid();

			while (sessions_.find(sessionid) != sessions_.end())
			{
				sessionid = make_sessionid();
			}

			s->set_sessionid(sessionid);

			if (s->start())
			{
				sessions_[sessionid] = s;
				sessionnum_++;
			}
		});
	}

	void io_worker::remove_session(uint32_t sessionid)
	{
		ios_.post([this, sessionid]() {
			remove(sessionid);
		});
	}

	void io_worker::close_session(uint32_t sessionid)
	{
		ios_.post([this, sessionid]() {
			auto iter = sessions_.find(sessionid);
			if (iter != sessions_.end())
			{
				iter->second->close();
			}
		});
	}

	void io_worker::send(uint32_t sessionid, const buffer_ptr_t& data)
	{
		ios_.post([this, sessionid, data]()
		{
			auto iter = sessions_.find(sessionid);
			if (iter != sessions_.end())
			{
				iter->second->send(data);
			}
		});
	}

	void io_worker::set_network_handle(const network_handle_t & handle)
	{
		handle_ = handle;
	}

	int io_worker::get_session_num()
	{
		return sessionnum_.load();
	}

	uint32_t io_worker::make_sessionid()
	{
		if (sessionuid_ == MAX_SESSION_UID)
			sessionuid_ = 1;
		auto id = sessionuid_++;
		auto sid = ((uint32_t(workerid_) << IO_WORKER_ID_SHIFT) | id);
		return sid;
	}

	void io_worker::start_checker()
	{
		checker_.expires_from_now(std::chrono::seconds(10));
		checker_.async_wait(std::bind(&io_worker::handle_checker, this, std::placeholders::_1));
	}

	void io_worker::handle_checker(const asio::error_code & e)
	{
		if (!e)
		{
			for (auto& it : sessions_)
			{
				it.second->check();
			}
			start_checker();
		}
		else
		{
			close_all_sessions();
		}
	}

	void io_worker::remove(uint32_t sessionid)
	{
		auto iter = sessions_.find(sessionid);
		if (iter != sessions_.end())
		{
			sessions_.erase(iter);
			sessionnum_--;
		}
	}

	void io_worker::close_all_sessions()
	{
		for (auto iter = sessions_.begin(); iter != sessions_.end();)
		{
			auto tmp = iter;
			iter++;
			tmp->second->close();
		}
		sessions_.clear();
	}
}