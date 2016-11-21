/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include "asio.hpp"
#include "config.h"
#include "common/buffer.hpp"

namespace moon
{
	enum class network_error
	{
		ok = 0,
		remote_endpoint = 1,// parse remote endpoint error
		message_size_max = 2, // send message size to long
		socket_shutdown = 3,//socket shutdown failed
		socket_close =4,// socket close failed
		socket_setoption = 5,//socket set option failed
		socket_read = 6, //socket read failed
		socket_read_timeout = 7, //socket read failed
		socket_send = 8, //socket send failed
	};

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

		void set_socket_handle(network_handle_t handle);

		void check();

		void set_sessionid(uint32_t id);

		std::function<void(uint32_t)> remove_self;
	private:
		bool ok();

		void	set_no_delay();

		void read();

		void send();

		void post_send(const buffer_ptr_t& data);

		void parse_remote_endpoint();

		void on_error(network_error ne, const asio::error_code& e);

		void on_connect();

		void on_message(const uint8_t* data, size_t len);

		void on_close();

	private:
		io_worker& worker_;
		asio::ip::tcp::socket socket_;
		asio::error_code error_code_;
		uint32_t sessionid_;
		int64_t last_recv_time_;
		uint32_t time_out_;
		std::vector<uint8_t> read_buffer_;
		bool	 is_sending_;
		buffer read_stream_;
		std::string remote_addr_;
		std::pair<network_error, std::string> network_error_;
		std::deque<buffer_ptr_t> send_queue_;
		message_head head_;
		network_handle_t handle_;
	};
}






