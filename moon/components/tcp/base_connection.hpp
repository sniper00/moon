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

    class tcp;

    class base_connection :public std::enable_shared_from_this<base_connection>
    {
    public:
        using socket_t = asio::ip::tcp::socket;

        using message_handler_t = std::function<void(const message_ptr_t&)>;

        template <typename... Args>
        explicit base_connection(tcp* t, Args&&... args)
            :sending_(false)
            , frame_flag_(frame_enable_flag::none)
            , logic_error_(network_logic_error::ok)
            , id_(0)
            , tcp_(t)
            , last_recv_time_(0)
            , socket_(std::forward<Args>(args)...)
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

            last_recv_time_ = now();
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

            if (send_queue_.size() >= WARN_NET_SEND_QUEUE_SIZE)
            {
                CONSOLE_DEBUG(logger(), "network send queue too long. size:%zu", send_queue_.size());
                if (send_queue_.size() >= MAX_NET_SEND_QUEUE_SIZE)
                {
                    logic_error_ = network_logic_error::send_message_queue_size_max;
                    close();
                    return false;
                }
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
        virtual void message_framing(const_buffers_holder& holder, buffer_ptr_t&& buf)
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
                if (msg->has_flag(buffer_flag::framing))
                {
                    message_framing(buffers_holder_, std::move(msg));
                }
                else
                {
                    buffers_holder_.push_back(std::move(msg));
                }
                send_queue_.pop_front();
            }

            if (buffers_holder_.size() == 0)
                return;

            sending_ = true;
            asio::async_write(
                socket_,
                buffers_holder_.buffers(),
                make_custom_alloc_handler(write_allocator_,
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
                        , (lerrmsg == nullptr ? logic_errmsg(lerrcode) : lerrmsg)
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
                handle_message(std::move(msg));
            }

            //closed
            {
                auto msg = message::create();
                msg->write_string(remote_addr_);
                msg->set_sender(id_);
                msg->set_subtype(static_cast<uint8_t>(socket_data_type::socket_close));
                handle_message(std::move(msg));
            }
        }

        template<typename TMsg>
        void handle_message(TMsg&& msg)
        {
            if (nullptr != tcp_)
            {
                msg->set_sender(id_);
                tcp_->handle_message(std::forward<TMsg>(msg));
            }
        }

        int64_t now()
        {
            if (nullptr != tcp_)
            {
                return tcp_->now();
            }
            else
            {
                return -1;
            }
        }
    protected:
        bool sending_;
        frame_enable_flag frame_flag_;
        network_logic_error logic_error_;
        uint32_t id_;
        tcp* tcp_;
        time_t last_recv_time_;
        socket_t socket_;
        handler_allocator read_allocator_;
        handler_allocator write_allocator_;
        const_buffers_holder  buffers_holder_;
        std::string remote_addr_;
        std::deque<buffer_ptr_t> send_queue_;
        moon::log* log_;
    };
}