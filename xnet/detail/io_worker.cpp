#include "io_worker.h"
#include "session.h"

using namespace moon;

io_worker::io_worker()
	:work_(ios_)
	,checker_(ios_)
	,workerid_(0)
	,sessionuid_(0)
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
		s->remove_self = std::bind(&io_worker::remove, this, std::placeholders::_1);
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

uint32_t io_worker::make_sessionid()
{
	auto id = ++sessionuid_;
	if (id == MAX_SESSION_NUM)
	{
		id = 1;
	}
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
		//CONSOLE_TRACE("worker %d, session num %u", workerid_, num);
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
