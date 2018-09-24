#pragma once
#include "config.h"
#include "asio.hpp"
#include "message.hpp"
#include "handler_alloc.hpp"
#include "const_buffers_holder.hpp"
#include "common/string.hpp"

namespace moon
{
    struct read_request
    {
        read_request()
            :delim(read_delim::CRLF)
            , size(0)
            , responseid(0)
        {

        }

        read_request(read_delim d, size_t s, int32_t r)
            :delim(d)
            , size(s)
            , responseid(r)
        {
        }

        read_request(const read_request&) = default;
        read_request& operator=(const read_request&) = default;

        read_delim delim;
        size_t size;
        int32_t responseid;
    };

    class base_connection :public std::enable_shared_from_this<base_connection>
    {
    public:
        using socket_t = asio::ip::tcp::socket;

        explicit base_connection(asio::io_service& ios, tcp* t)
            :sending_(false)
            , frame_flag_(frame_enable_flag::none)
            , id_(0)
            , last_recv_time_(0)
            , logic_error_(network_logic_error::ok)
            , ios_(ios)
            , tcp_(t)
            , socket_(ios)
            , log_(nullptr)
        {
        }

        base_connection(const base_connection&) = delete;

        base_connection& operator=(const base_connection&) = delete;

        virtual ~base_connection()
        {
        }

        virtual void start(bool accepted, int32_t responseid = 0)
        {
            (void)accepted;
            (void)responseid;
            //save remote addr
            asio::error_code ec;
            auto ep = socket_.remote_endpoint(ec);
            auto addr = ep.address();
            remote_addr_ = addr.to_string(ec) + ":";
            remote_addr_ += std::to_string(ep.port());

            last_recv_time_ = std::time(nullptr);
        }

        virtual bool read(const read_request& ctx)
        {
            (void)ctx;
            return false;
        };

        virtual bool send(const buffer_ptr_t & data)
        {
            if (data == nullptr || data->size() == 0)
            {
                return false;
            }

            if (!socket_.is_open())
            {
                return false;
            }

            send_queue_.push_back(data);

            if (send_queue_.size() > 30)
            {
                CONSOLE_WARN(logger(), "network send_queue too long %zu", send_queue_.size());
            }

            if (!sending_)
            {
                post_send();
            }
            return true;
        }

        void close(bool exit = false)
        {
            if (socket_.is_open())
            {
                asio::error_code ignore_ec;
                socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignore_ec);
                socket_.close(ignore_ec);
            }

            if (exit)
            {
                tcp_ = nullptr;
            }
        }

        socket_t& socket()
        {
            return socket_;
        }

        bool is_open() const
        {
            return socket_.is_open();
        }

        void set_id(uint32_t id)
        {
            id_ = id;
        }

        uint32_t id() const
        {
            return id_;
        }

        void timeout_check(time_t now, int timeout)
        {
            if ((0 != timeout) && (0 != last_recv_time_) && (now - last_recv_time_ > timeout))
            {
                logic_error_ = network_logic_error::timeout;
                close();
            }
        }

        void set_no_delay()
        {
            asio::ip::tcp::no_delay option(true);
            asio::error_code ec;
            socket_.set_option(option, ec);
        }

        moon::log* logger() const
        {
            return log_;
        }

        void logger(moon::log* l)
        {
            log_ = l;
        }

        void set_enable_frame(frame_enable_flag t)
        {
            frame_flag_ = t;
        }

    protected:
        virtual void message_framing(const_buffers_holder& holder, const buffer_ptr_t& buf)
        {
			(void)holder;
			(void)buf;
        }

        void post_send()
        {
            buffers_holder_.clear();
            if (send_queue_.size() == 0)
                return;

            while ((send_queue_.size() != 0) && (buffers_holder_.size() < 50))
            {
                auto& msg = send_queue_.front();
                if (msg->has_flag(static_cast<uint8_t>(buffer_flag::framing)))
                {
                    message_framing(buffers_holder_, msg);
                }
                else
                {
                    buffers_holder_.push_back(msg);
                }
                send_queue_.pop_front();
            }

            if (buffers_holder_.size() == 0)
                return;

            sending_ = true;
            asio::async_write(
                socket_,
                buffers_holder_.buffers(),
                make_custom_alloc_handler(allocator_,
                    [this, self = shared_from_this()](const asio::error_code& e, std::size_t)
            {
                sending_ = false;

                if (!e)
                {
                    if (buffers_holder_.close())
                    {
                        close();
                    }
                    else
                    {
                        post_send();
                    }
                }
                else
                {
                    error(e, int(logic_error_));
                }
            }));
        }

        virtual void error(const asio::error_code& e, int lerrcode, const char* lerrmsg = nullptr)
        {
            //error
            {
                auto msg = message::create();
                std::string content;
                if (lerrcode)
                {
                    content = moon::format("{\"addr\":\"%s\",\"logic_errcode\":%d,\"errmsg\":\"%s\"}"
                        , remote_addr_.data()
                        , lerrcode
                        , (lerrmsg==nullptr?logic_errmsg(lerrcode): lerrmsg)
                    );
					msg->set_subtype(static_cast<uint8_t>(socket_data_type::socket_error));
                }
				else if (e && e != asio::error::eof)
				{
					content = moon::format("{\"addr\":\"%s\",\"errcode\":%d,\"errmsg\":\"%s\"}"
						, remote_addr_.data()
						, e.value()
						, e.message().data());
					msg->set_subtype(static_cast<uint8_t>(socket_data_type::socket_error));
				}

                msg->write_string(content);
                msg->set_sender(id_);
                msg->set_type(PTYPE_SOCKET);
                handle_message(msg);
            }

            //closed
            {
                auto msg = message::create();
                msg->write_string(remote_addr_);
                msg->set_sender(id_);
                msg->set_subtype(static_cast<uint8_t>(socket_data_type::socket_close));
                msg->set_type(PTYPE_SOCKET);
                handle_message(msg);
            }
            remove(id_);
        }

        void remove(uint32_t connid)
        {
            if (nullptr != tcp_)
            {
                tcp_->close(connid);
                tcp_ = nullptr;
            }
        }

        void handle_message(const message_ptr_t& msg)
        {
            if (nullptr != tcp_)
            {
                tcp_->handle_message(msg);
            }
        }

    protected:
        bool sending_;
        frame_enable_flag frame_flag_;
        uint32_t id_;
        time_t last_recv_time_;
        network_logic_error logic_error_;
        asio::io_service& ios_;
        tcp* tcp_;
        socket_t socket_;
        handler_allocator allocator_;
        const_buffers_holder  buffers_holder_;
        std::string remote_addr_;
        std::deque<buffer_ptr_t> send_queue_;
        moon::log* log_;
    };
}