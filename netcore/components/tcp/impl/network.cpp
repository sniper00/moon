/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#include "components/tcp/network.h"
#include "io_service_pool.h"
#include "io_worker.h"
#include "session.h"
#include "common/sync_queue.hpp"
#include "log.h"
#include "asio.hpp"

namespace moon
{
	struct  network::network_imp
	{
		network_imp(int nthread, uint32_t timeout)
			:pool_(nthread)
			, acceptor_(pool_.io_service())
			, signals_(pool_.io_service())
			, time_out_(timeout)
			, running_(false)
		{

		}

		void stop()
		{
			if (!running_)
				return;

			running_ = false;

			if (acceptor_.is_open())
			{
				acceptor_.close(error_code_);
			}

			queue_.exit();
			pool_.stop();
		}

		void error(const asio::error_code& e)
		{
			CONSOLE_ERROR("network error %s(%d)", e.message().data(), e.value());
		}

		bool	running_;
		io_service_pool pool_;
		asio::ip::tcp::acceptor acceptor_;
		asio::signal_set signals_;
		asio::error_code error_code_;
		uint32_t time_out_;
		int session_num_;
		network_handle_t handle_;
		sync_queue<message_ptr_t> queue_;
	};

	network::network()
		:network_imp_(nullptr)
	{
		
	}

	network::~network()
	{
		destory();
		SAFE_DELETE(network_imp_);
	}

	void network::init(int nthread, uint32_t timeout)
	{
		SAFE_DELETE(network_imp_);
		network_imp_ = new network_imp(nthread, timeout);
	}

	void network::set_max_queue_size(size_t size)
	{
		network_imp_->queue_.set_max_size(size);
	}

	void network::set_network_handle(const network_handle_t & handle)
	{
		network_imp_->handle_ = handle;
		network_imp_->pool_.set_network_handle(handle);
	}

	void network::start()
	{
		if (network_imp_->running_)
			return;

		if (!ok())
		{
			return;
		}

		network_imp_->pool_.set_network_handle([this](const message_ptr_t& m) {
			network_imp_->queue_.push_back(m);
		});

		network_imp_->pool_.run();
		start_accept();

		network_imp_->running_ = true;
	}

	void network::destory()
	{
		if (network_imp_ != nullptr)
		{
			network_imp_->stop();
		}
	}

	void network::listen(const std::string & ip, const std::string & port)
	{
		auto& pool_ = network_imp_->pool_;
		auto& error_code_ = network_imp_->error_code_;
		auto& acceptor_ = network_imp_->acceptor_;

		asio::ip::tcp::resolver resolver(pool_.io_service());
		asio::ip::tcp::resolver::query query(ip, port);
		resolver.resolve(query, error_code_);
		auto iter = resolver.resolve(query, error_code_);
		if (error_code_)
		{
			network_imp_->error(error_code_);
			return;
		}
		asio::ip::tcp::endpoint endpoint = *iter;

		acceptor_.open(endpoint.protocol(), error_code_);
		if (error_code_)
		{
			network_imp_->error(error_code_);
			return;
		}

		acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true), error_code_);
		if (error_code_)
		{
			network_imp_->error(error_code_);
			return;
		}

		acceptor_.bind(endpoint, error_code_);
		if (error_code_)
		{
			network_imp_->error(error_code_);
			return;
		}

		acceptor_.listen(std::numeric_limits<int>::max(), error_code_);
		if (error_code_)
		{
			network_imp_->error(error_code_);
		}
	}

	void network::start_accept()
	{
		auto& pool_ = network_imp_->pool_;
		auto& acceptor_ = network_imp_->acceptor_;

		if (!ok() || !acceptor_.is_open())
			return;
		auto& worker = pool_.get_io_worker();
		auto s = worker->create_session(network_imp_->time_out_);

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
				network_imp_->error(e);
			}
		});
	}

	void network::async_connect(const std::string & ip, const std::string & port)
	{
		auto& pool_ = network_imp_->pool_;
		auto& error_code_ = network_imp_->error_code_;
		auto& acceptor_ = network_imp_->acceptor_;

		asio::ip::tcp::resolver resolver(pool_.io_service());
		asio::ip::tcp::resolver::query query(ip, port);
		asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query, error_code_);
		if (error_code_)
		{
			network_imp_->error(error_code_);
			return;
		}

		auto& worker = pool_.get_io_worker();
		auto s = worker->create_session(network_imp_->time_out_);
		asio::async_connect(s->socket(), endpoint_iterator,
			[this, s, &worker, ip, port](const asio::error_code& e, asio::ip::tcp::resolver::iterator)
		{
			if (!e)
			{
				worker->add_session(s);
			}
			else
			{
				network_imp_->error(e);
			}
		});
	}

	uint32_t network::sync_connect(const std::string & ip, const std::string & port)
	{
		auto& pool_ = network_imp_->pool_;
		auto& error_code_ = network_imp_->error_code_;
		auto& acceptor_ = network_imp_->acceptor_;

		asio::ip::tcp::resolver resolver(pool_.io_service());
		asio::ip::tcp::resolver::query query(ip, port);
		asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query, error_code_);
		if (error_code_)
		{
			network_imp_->error(error_code_);
			return 0;
		}

		auto& worker = pool_.get_io_worker();
		auto s = worker->create_session(network_imp_->time_out_);

		asio::connect(s->socket(), endpoint_iterator, error_code_);
		if (error_code_)
		{
			network_imp_->error(error_code_);
			return 0;
		}

		return s->sessionid();
	}

	void network::send(uint32_t sessionid, const buffer_ptr_t& data)
	{
		if (!data->check_flag(uint8_t(buffer_flag::pack_size)))
		{
			uint16_t size = data->size();
			data->write_front(&size, 0, 1);
			data->set_flag(uint8_t(buffer_flag::pack_size));
		}

		auto worker = network_imp_->pool_.find(sessionid);
		if (nullptr != worker)
		{
			worker->send(sessionid, data);
		}
	}

	void network::close(uint32_t sessionid)
	{
		auto worker = network_imp_->pool_.find(sessionid);
		if (nullptr != worker)
		{
			worker->close_session(sessionid);
		}
	}

	int network::get_session_num()
	{
		return network_imp_->pool_.get_session_num();
	}

	void network::update()
	{
		auto datas = network_imp_->queue_.move();
		for (auto& d : datas)
		{
			network_imp_->handle_(d);
		}
	}

	bool network::ok()
	{
		return (!network_imp_->error_code_);
	}
}
