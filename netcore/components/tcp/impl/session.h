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

	class const_buffers_holder
	{
	public:
		const_buffers_holder() = default;

		const_buffers_holder(const buffer_ptr_t& buf)
		{
			push_back(buf);
		}

		void push_back(const buffer_ptr_t& buf)
		{
			buffers_.push_back(asio::buffer(buf->data(), buf->size()));
			datas_.push_back(buf);
		}

		const auto& buffers() const
		{
			return buffers_;
		}

		size_t size()
		{
			return datas_.size();
		}

		void clear()
		{
			buffers_.clear();
			datas_.clear();
		}
	private:
		std::vector<asio::const_buffer> buffers_;
		std::vector<buffer_ptr_t> datas_;
	};

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

		void read_header();

		void read_body(message_size_t size);

		void do_send();

		void post_send();

		void parse_remote_endpoint();

		void on_network_error(const asio::error_code& e);

		void on_logic_error(network_logic_error ne);

		void on_connect();

		void on_message(const buffer_ptr_t& buf);

		void on_close();

	private:
		io_worker& worker_;
		asio::ip::tcp::socket socket_;
		asio::error_code error_code_;
		uint32_t sessionid_;
		time_t last_recv_time_;
		uint32_t time_out_;
		message_size_t msg_size_;
		bool	 is_sending_;
		std::string remote_addr_;
		std::deque<buffer_ptr_t> send_queue_;
		const_buffers_holder  buffers_holder_;
		handler_allocator allocator_;
	};
}






