/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#include "network.h"
#include "io_service_pool.h"
#include "io_worker.h"
#include "session.h"
#include "common/sync_queue.hpp"
#include "log.h"

namespace moon
{
	network::network(uint8_t pool_size, uint32_t time_out)
		:pool_(pool_size)
		, acceptor_(pool_.io_service())
		, signals_(pool_.io_service())
		, time_out_(time_out)
		, running_(false)
	{
		
	}

	network::~network()
	{
		stop();
	}

	void network::set_max_queue_size(size_t size)
	{
		queue_.set_max_size(size);
	}

	void network::set_network_handle(const network_handle_t & handle)
	{
		handle_ = handle;
		pool_.set_network_handle(handle);
	}

	void network::run()
	{
		if (running_)
			return;

		if (!ok())
		{
			return;
		}

		pool_.set_network_handle([this](const message_ptr_t& m) {
			queue_.push_back(m);
		});

		pool_.run();
		start_accept();

		running_ = true;
	}

	void network::stop()
	{
		if (!running_)
			return;
		if (acceptor_.is_open())
		{
			acceptor_.close(error_code_);
		}

		pool_.stop();
		running_ = false;
	}

	void network::listen(const std::string & ip, const std::string & port)
	{
		asio::ip::tcp::resolver resolver(pool_.io_service());
		asio::ip::tcp::resolver::query query(ip, port);
		resolver.resolve(query, error_code_);
		auto iter = resolver.resolve(query, error_code_);
		if (error_code_)
		{
			error(error_code_);
			return;
		}
		asio::ip::tcp::endpoint endpoint = *iter;

		acceptor_.open(endpoint.protocol(), error_code_);
		if (error_code_)
		{
			error(error_code_);
			return;
		}

		acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true), error_code_);
		if (error_code_)
		{
			error(error_code_);
			return;
		}

		acceptor_.bind(endpoint, error_code_);
		if (error_code_)
		{
			error(error_code_);
			return;
		}

		acceptor_.listen(std::numeric_limits<int>::max(), error_code_);
		if (error_code_)
		{
			error(error_code_);
		}
	}

	void network::start_accept()
	{
		if (!ok() || !acceptor_.is_open())
			return;
		auto& worker = pool_.get_io_worker();
		auto s = worker->create_session(time_out_);

		acceptor_.async_accept(s->socket(), [this, s, &worker](const asio::error_code& e) {
			if (!ok())
				return;

			if (!e)
			{
				worker->add_session(s);
				start_accept();
			}
			else
			{
				error(e);
			}
		});
	}

	void network::async_connect(const std::string & ip, const std::string & port)
	{
		asio::ip::tcp::resolver resolver(pool_.io_service());
		asio::ip::tcp::resolver::query query(ip, port);
		asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query, error_code_);
		if (error_code_)
		{
			error(error_code_);
			return;
		}

		auto& worker = pool_.get_io_worker();
		auto s = worker->create_session(time_out_);
		asio::async_connect(s->socket(), endpoint_iterator,
			[this, s, &worker, ip, port](const asio::error_code& e, asio::ip::tcp::resolver::iterator)
		{
			if (!e)
			{
				worker->add_session(s);
			}
			else
			{
				error(e);
			}
		});
	}

	uint32_t network::sync_connect(const std::string & ip, const std::string & port)
	{
		asio::ip::tcp::resolver resolver(pool_.io_service());
		asio::ip::tcp::resolver::query query(ip, port);
		asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query, error_code_);
		if (error_code_)
		{
			error(error_code_);
			return 0;
		}

		auto& worker = pool_.get_io_worker();
		auto s = worker->create_session(time_out_);

		asio::connect(s->socket(), endpoint_iterator, error_code_);
		if (error_code_)
		{
			error(error_code_);
			return 0;
		}

		return s->sessionid();
	}

	void network::send(uint32_t sessionid, const buffer_ptr_t& data)
	{
		auto worker = pool_.find(sessionid);
		if (nullptr != worker)
		{
			worker->send(sessionid, data);
		}
	}

	void network::close(uint32_t sessionid)
	{
		auto worker = pool_.find(sessionid);
		if (nullptr != worker)
		{
			worker->close_session(sessionid);
		}
	}

	void network::update()
	{
		auto datas = queue_.move();
		for (auto& d : datas)
		{
			handle_(d);
		}
	}

	void network::error(const asio::error_code & e)
	{
		CONSOLE_ERROR("network error %s(%d)", e.message().data(), e.value());
	}

	bool network::ok()
	{
		return (!error_code_);
	}
}
