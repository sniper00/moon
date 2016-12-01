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

		while (!ios_.stopped())
		{
			thread_sleep(10);
		}
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

			while (sessions_[uint16_t(sessionid)] != nullptr)
			{
				sessionid = make_sessionid();
			}
			s->set_sessionid(sessionid);

			if (s->start())
			{
				sessions_[uint16_t(sessionid)] = s;
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
			if (uint16_t(sessionid) < MAX_SESSION_NUM && sessions_[uint16_t(sessionid)] != nullptr)
			{
				sessions_[uint16_t(sessionid)]->close();
			}
		});
	}

	void io_worker::send(uint32_t sessionid, const buffer_ptr_t& data)
	{
		ios_.post([this, sessionid, data]()
		{
			if (uint16_t(sessionid) < MAX_SESSION_NUM && sessions_[uint16_t(sessionid)] != nullptr)
			{
				sessions_[uint16_t(sessionid)]->send(data);
			}
		});
	}

	void io_worker::set_network_handle(const network_handle_t & handle)
	{
		handle_ = handle;
	}

	uint32_t io_worker::make_sessionid()
	{
		if (sessionuid_ == MAX_SESSION_NUM)
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
			auto cursecond = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			int num = 0;
			for (size_t i = 0; i < MAX_SESSION_NUM; i++)
			{
				if (sessions_[i] != nullptr)
				{
					sessions_[i]->check();
					num++;
				}
			}
			start_checker();
			//report(num);
		}
		else
		{
			close_all_sessions();
		}
	}

	void io_worker::remove(uint32_t sessionid)
	{
		if (uint16_t(sessionid) < MAX_SESSION_NUM)
		{
			sessions_[uint16_t(sessionid)].reset();
		}
	}

	void io_worker::close_all_sessions()
	{
		for (size_t i = 0; i < MAX_SESSION_NUM; i++)
		{
			if (sessions_[i] != nullptr)
			{
				sessions_[i]->close();
				sessions_[i].reset();
			}
		}
	}

	void io_worker::report(size_t session_num)
	{
		auto str = moon::format(R"({"io_worker_id":%d,"session_num":%llu})", workerid_, session_num);
		auto msg = message::create(str.size());
		msg->to_buffer()->write_back(str.data(), 0, str.size()+1);
		msg->set_type(message_type::system_network_report);
		msg->set_broadcast(true);
		handle_(msg);
	}
}