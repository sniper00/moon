#include "session.h"
#include "io_worker.h"
#include "common/buffer_writer.hpp"
#include "common/string.hpp"
#include "message.h"
#include "log.h"

namespace moon
{
	session::session(io_worker & worker, uint32_t id, uint32_t time_out)
		:worker_(worker)
		, socket_(worker.io_service())
		, sessionid_(id)
		, last_recv_time_(0)
		, time_out_(time_out)
		, msg_size_(0)
		, is_sending_(false)
	{
	}

	session::~session()
	{
	}

	bool session::start()
	{
		parse_remote_endpoint();
		if (!ok())
		{
			return false;
		}

		on_connect();

		read_header();

		return true;
	}


	bool session::ok()
	{
		return (socket_.is_open() && !error_code_);
	}

	void session::set_no_delay()
	{
		asio::ip::tcp::no_delay option(true);
		asio::error_code ec;
		socket_.set_option(option, ec);
	}

	void session::read_header()
	{
		auto self = shared_from_this();
		asio::async_read(socket_, asio::buffer(&msg_size_, sizeof(msg_size_)),
			make_custom_alloc_handler(allocator_,
			[this,self](const asio::error_code& e, std::size_t bytes_transferred)
		{
			if (!ok())
			{
				return;
			}

			if (e)
			{
				on_network_error(e);
				return;
			}

			last_recv_time_ = std::time(nullptr);

			if (bytes_transferred == 0)
			{
				read_header();
				return;
			}

			if (msg_size_ > MAX_NMSG_SIZE)
			{
				on_logic_error(network_logic_error::message_size_max);
				return;
			}

			read_body(msg_size_);
		}));
	}

	void session::read_body(message_size_t size)
	{
		auto self = shared_from_this();
		auto buf = create_buffer(size);
		asio::async_read(socket_, asio::buffer((void*)buf->data(),size),
			make_custom_alloc_handler(allocator_,
				[this, self, buf,size](const asio::error_code& e, std::size_t bytes_transferred)
		{
			if (!ok())
			{
				return;
			}

			if (e)
			{
				on_network_error(e);
				return;
			}

			if (bytes_transferred == 0)
			{
				read_header();
				return;
			}

			buf->offset_writepos(size);

			on_message(buf);

			read_header();
		}));
	}

	void session::send(const buffer_ptr_t& data)
	{
		if (data->size() > MAX_NMSG_SIZE)
		{
			on_logic_error(network_logic_error::message_size_max);
			return;
		}

		if (data == nullptr || data->size() == 0)
		{
			assert(0);
			return;
		}

		send_queue_.push_back(data);

		if (send_queue_.size() > 5)
		{
			CONSOLE_WARN("session send queue > 5");
		}

		if (!is_sending_)
		{
			do_send();
		}
	}

	void session::do_send()
	{
		if (!ok())
		{
			return;
		}

		if (send_queue_.size() == 0)
			return;

		buffers_holder_.clear();

		while ((send_queue_.size() !=0) && (buffers_holder_.size()<10))
		{
			auto& msg = send_queue_.front();
			buffers_holder_.push_back(msg);
			send_queue_.pop_front();
		}

		if (buffers_holder_.size() == 0)
			return;

		post_send();
	}

	void moon::session::post_send()
	{
		is_sending_ = true;
		auto self = shared_from_this();
		asio::async_write(
			socket_,
			buffers_holder_.buffers(),
			make_custom_alloc_handler(allocator_,
				[this, self](const asio::error_code& e, std::size_t bytes_transferred)
		{
			is_sending_ = false;
			if (!ok())
				return;
			if (!e)
			{
				do_send();
				return;
			}
			else
			{
				on_network_error(e);
			}
		}
		));
	}

	void session::parse_remote_endpoint()
	{
		auto ep = socket_.remote_endpoint(error_code_);
		if (error_code_) return;
		auto addr = ep.address();
		remote_addr_ = addr.to_string(error_code_) + ":";
		remote_addr_ += std::to_string(ep.port());
	}

	void session::on_network_error(const asio::error_code& e)
	{
		auto msg = message::create();
		buffer_writer<buffer> bw(*msg);
		bw << remote_addr_;
		bw << int32_t(e.value());
		bw << e.message();
		msg->set_sender(sessionid_);
		msg->set_type(message_type::network_error);
		worker_(msg);
		close();
	}

	void session::on_logic_error(network_logic_error ne)
	{
		auto msg = message::create();
		buffer_writer<buffer> bw(*msg);
		bw << remote_addr_;
		bw << int32_t(ne);
		msg->set_sender(sessionid_);
		msg->set_type(message_type::network_logic_error);
		worker_(msg);
		close();
	}

	void session::on_connect()
	{
		auto msg = message::create();
		buffer_writer<buffer> bw(*msg);
		bw << remote_addr_;
		msg->set_sender(sessionid_);
		msg->set_type(message_type::network_connect);
		worker_(msg);
	}

	void session::on_message(const buffer_ptr_t& buf)
	{
		auto msg = message::create(buf);
		msg->set_sender(sessionid_);
		msg->set_type(message_type::network_recv);
		worker_(msg);
	}

	void session::on_close()
	{
		auto msg = message::create();
		buffer_writer<buffer> bw(*msg);
		bw << remote_addr_;
		msg->set_sender(sessionid_);
		msg->set_type(message_type::network_close);
		worker_(msg);
		worker_.remove_session(sessionid_);
	}

	asio::ip::tcp::socket& session::socket()
	{
		return socket_;
	};

	void session::close()
	{
		on_close();
		if (socket_.is_open())
		{
			socket_.shutdown(asio::ip::tcp::socket::shutdown_both, error_code_);
			if (error_code_)
			{
				return;
			}
			socket_.close(error_code_);
			if (error_code_)
			{
				return;
			}
		}
	}

	uint32_t session::sessionid()
	{
		return sessionid_;
	}

	void session::set_sessionid(uint32_t v)
	{
		sessionid_ = v;
	}

	void session::check()
	{
		auto curtime = std::time(nullptr);
		if (( 0 != time_out_ )&&(0 != last_recv_time_)&&(curtime - last_recv_time_ > time_out_))
		{
			on_logic_error(network_logic_error::socket_read_timeout);
		}
	}
}
