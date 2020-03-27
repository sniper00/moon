#pragma once
#include "config.hpp"
#include "asio.hpp"
#include "message.hpp"
#include "handler_alloc.hpp"
#include "const_buffers_holder.hpp"
#include "common/string.hpp"
#include "error.hpp"

namespace moon
{
    class base_connection :public std::enable_shared_from_this<base_connection>
    {
    public:
        using socket_t = asio::ip::tcp::socket;

        using message_handler_t = std::function<void(const message_ptr_t&)>;

        template <typename... Args>
        explicit base_connection(uint32_t serviceid, uint8_t type, moon::socket* s, Args&&... args)
            : serviceid_(serviceid)
            , type_(type)
            , parent_(s)
            , socket_(std::forward<Args>(args)...)
        {
        }

        base_connection(const base_connection&) = delete;

        base_connection& operator=(const base_connection&) = delete;

        virtual ~base_connection()
        {
        }

        virtual void start(bool accepted)
        {
            (void)accepted;
            recvtime_ = now();
        }

        virtual void read(size_t, std::string_view, int32_t)
        {
            assert(false);
        };

        virtual bool send(buffer_ptr_t data)
        {
            if (data == nullptr || data->size() == 0)
            {
                return false;
            }

            if (!socket_.is_open())
            {
                return false;
            }

            queue_.emplace_back(std::move(data));

            if (wq_warn_size_ != 0 && queue_.size() >= wq_warn_size_)
            {
                CONSOLE_DEBUG(logger(), "network send queue too long. size:%zu", queue_.size());
                if (wq_error_size_ != 0 && queue_.size() >= wq_error_size_)
                {
                    error(make_error_code(moon::error::send_queue_too_big));
                    return false;
                }
            }

            if (!sending_)
            {
                post_send();
            }
            return true;
        }

        void close()
        {
            if (socket_.is_open())
            {
                asio::error_code ignore_ec;
                socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignore_ec);
                socket_.close(ignore_ec);
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

        void fd(uint32_t fd)
        {
            fd_ = fd;
        }

        uint32_t fd() const
        {
            return fd_;
        }

        void timeout(time_t now)
        {
            if ((0 != timeout_) && (0 != recvtime_) && (now - recvtime_ > timeout_))
            {
                asio::post(socket_.get_executor(), [this]() {
                    error(make_error_code(moon::error::read_timeout));
                });
                return;
            }
            return;
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

        void settimeout(uint32_t v)
        {
            timeout_ = v;
        }

        void set_send_queue_limit(uint32_t warnsize, uint32_t errorsize)
        {
            wq_warn_size_ = warnsize;
            wq_error_size_ = errorsize;
        }

        static time_t now()
        {
            return std::time(nullptr);
        }

        std::string address()
        {
            std::string address;
            asio::error_code ec;
            auto endpoint = socket_.remote_endpoint(ec);
            if (!ec)
            {
                address.append(endpoint.address().to_string());
                address.append(":");
                address.append(std::to_string(endpoint.port()));
            }
            return address;
        }
    protected:
        virtual void message_slice(const_buffers_holder& holder, const buffer_ptr_t& buf)
        {
            (void)holder;
            (void)buf;
        }

        void post_send()
        {
            if (queue_.size() == 0)
                return;

            for (const auto& buf : queue_)
            {
                if (buf->has_flag(buffer_flag::chunked))
                {
                    message_slice(holder_, buf);
                }
                else
                {
                    holder_.push_back(buf->data(), buf->size(), buf->has_flag(buffer_flag::close));
                }

                if (holder_.size() >= const_buffers_holder::max_count)
                {
                    break;
                }
            }

            sending_ = true;
            asio::async_write(
                socket_,
                make_buffers_ref(holder_.buffers()),
                make_custom_alloc_handler(wallocator_,
                    [this, self = shared_from_this()](const asio::error_code& e, std::size_t)
            {
                sending_ = false;

                if (!e)
                {
                    if (holder_.close())
                    {
                        close();
                    }
                    else
                    {
                        for (size_t i = 0; i < holder_.count(); ++i)
                        {
                            queue_.pop_front();
                        }

                        holder_.clear();

                        post_send();
                    }
                }
                else
                {
                    error(e);
                }
            }));
        }

        virtual void error(const asio::error_code& e, const std::string& additional = "")
        {
            if (nullptr == parent_)
            {
                return;
            }

            //error
            {
                if (e && e != asio::error::eof)
                {
                    auto msg = message::create();
                    std::string message = e.message();
                    if (!additional.empty())
                    {
                        message.append("(");
                        message.append(additional);
                        message.append(")");
                    }
                    std::string content = moon::format("{\"addr\":\"%s\",\"errcode\":%d,\"errmsg\":\"%s\"}"
                        , address().data()
                        , e.value()
                        , e.message().data());
                    msg->set_subtype(static_cast<uint8_t>(socket_data_type::socket_error));
                    msg->write_data(content);
                    msg->set_sender(fd_);
                    handle_message(std::move(msg));
                }
            }

            //closed
            {
                auto msg = message::create();
                msg->write_data(address());
                msg->set_sender(fd_);
                msg->set_subtype(static_cast<uint8_t>(socket_data_type::socket_close));
                handle_message(std::move(msg));
                parent_->close(fd_, true);
            }
            parent_ = nullptr;
        }

        template<typename Message>
        void handle_message(Message&& m)
        {
            if (nullptr != parent_)
            {
                m->set_sender(fd_);
                if (m->type() == 0)
                {
                    m->set_type(type_);
                }
                parent_->handle_message(serviceid_, std::forward<Message>(m));
            }
        }
    protected:
        bool sending_ = false;
        uint32_t fd_ = 0;
        time_t recvtime_ = 0;
        uint32_t timeout_ = 0;
        moon::log* log_ = nullptr;
        uint32_t wq_warn_size_ = 0;
        uint32_t wq_error_size_ = 0;
        uint32_t serviceid_;
        uint8_t type_;
        moon::socket* parent_;
        socket_t socket_;
        handler_allocator rallocator_;
        handler_allocator wallocator_;
        const_buffers_holder  holder_;
        std::deque<buffer_ptr_t> queue_;
    };
}