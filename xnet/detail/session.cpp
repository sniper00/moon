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
		, time_out_(time_out_)
		, is_sending_(false)
		, read_buffer_(IO_BUFFER_SIZE)
	{
		//CONSOLE_TRACE("session create %u", sessionid_);
	}

	session::~session()
	{
		//CONSOLE_TRACE("session delete %u", sessionid_);
	}

	bool session::start()
	{
		parse_remote_endpoint();
		if (!ok())
		{
			return false;
		}

		on_connect();

		read();

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

	void session::read()
	{
		auto self = shared_from_this();
		socket_.async_read_some(asio::buffer(read_buffer_),
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
				read();
				return;
			}

			read_stream_.write_back(read_buffer_.data(), 0, bytes_transferred);
			while (read_stream_.size() > sizeof(message_head))
			{
				message_head* phead = (message_head*)(read_stream_.data());
				if (phead->len > MAX_NMSG_SIZE)
				{
					on_logic_error(network_logic_error::message_size_max);
					return;
				}

				if (read_stream_.size() < sizeof(message_head) + phead->len)
				{
					break;
				}

				read_stream_.seek(sizeof(message_head), buffer::Current);
				on_message(read_stream_.data(), phead->len);
				read_stream_.seek(phead->len, buffer::Current);
			}
			read();
		});
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

		if (!is_sending_)
		{
			post_send(data);
		}
		else
		{
			send_queue_.push_back(data);
			if (send_queue_.size() > 10)
			{
				CONSOLE_WARN("session send queue > 5");
			}
		}
	}

	void session::send()
	{
		if (!ok())
		{
			return;
		}

		if (send_queue_.size() == 0)
			return;
		auto data = send_queue_.front();
		send_queue_.pop_front();
		while ((send_queue_.size() > 0) && (data->size() + send_queue_.front()->size() <= IO_BUFFER_SIZE))
		{
			auto& msg = send_queue_.front();
			data->write_back(msg->data(), 0, msg->size());
			send_queue_.pop_front();
		}

		if (data->size() == 0)
			return;
		post_send(data);
	}

	void session::post_send(const buffer_ptr_t& data)
	{
		uint16_t  len = data->size();
		data->write_front(&len, 0, 1);

		is_sending_ = true;
		auto self = shared_from_this();
		asio::async_write(
			socket_,
			asio::buffer(data->data(), data->size()),
			[this,self, data](const asio::error_code& e, std::size_t bytes_transferred)
		{
			if (!ok())
				return;
			is_sending_ = false;
			if (!e)
			{
				send();
				return;
			}
			else
			{
				on_network_error(e);
			}
		});
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

	void session::on_message(const uint8_t * data, size_t len)
	{
		auto msg = message::create(len);
		buffer_writer<buffer> bw(*msg);
		bw.write_array(data, len);
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
		worker_.remove(sessionid_);
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
		if (time_out_ != 0 && (curtime - last_recv_time_ > time_out_))
		{
			on_logic_error(network_logic_error::socket_read_timeout);
		}
	}
}