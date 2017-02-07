/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include "asio.hpp"
#include "components/tcp/network.h"
#include "common/buffer.hpp"
#include "handler_alloc.hpp"
namespace moon
{
	class io_worker;
	DECLARE_SHARED_PTR(buffer)

	class session :public std::enable_shared_from_this<session>,asio::noncopyable
	{	
	public:
		session(io_worker& worker, uint32_t id, uint32_t time_out);

		~session();

		bool	start();

		void send(const buffer_ptr_t& data);

		asio::ip::tcp::socket& socket();

		void close();

		uint32_t sessionid();

		void check();

		void set_sessionid(uint32_t id);

	private:
		bool ok();

		void	set_no_delay();

		void read();

		void send();

		void post_send(const buffer_ptr_t& data);

		void parse_remote_endpoint();

		void on_network_error(const asio::error_code& e);

		void on_logic_error(network_logic_error ne);

		void on_connect();

		void on_message(const uint8_t* data, size_t len);

		void on_close();

	private:
		io_worker& worker_;
		asio::ip::tcp::socket socket_;
		asio::error_code error_code_;
		uint32_t sessionid_;
		time_t last_recv_time_;
		uint32_t time_out_;
		std::vector<uint8_t> read_buffer_;
		bool	 is_sending_;
		buffer read_stream_;
		std::string remote_addr_;
		std::deque<buffer_ptr_t> send_queue_;
		handler_allocator allocator_;
	};
}






