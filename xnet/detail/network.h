/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include "config.h"
#include "asio.hpp"
#include "io_service_pool.h"
#include "common/sync_queue.hpp"

namespace moon
{
	class network
	{
	public:
		network(uint8_t pool_size, uint32_t time_out);

		~network();

		void set_max_queue_size(size_t size);

		void	set_network_handle(const network_handle_t& handle);

		void run();

		void stop();

		void listen(const std::string& ip, const std::string& port);

		void async_connect(const std::string & ip, const std::string & port);

		uint32_t sync_connect(const std::string & ip, const std::string & port);

		void send(uint32_t sessionid,const buffer_ptr_t& data);

		void close(uint32_t sessionid);

		void update();

	private:
		void start_accept();

		void error(const asio::error_code& e);

		bool ok();
	private:
		bool	running_;
		io_service_pool pool_;
		asio::ip::tcp::acceptor acceptor_;
		asio::signal_set signals_;
		asio::error_code error_code_;
		uint32_t time_out_;
		network_handle_t handle_;
		sync_queue<message_ptr_t> queue_;
	};
};
