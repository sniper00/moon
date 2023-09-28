#pragma once
#include "config.hpp"
#include "asio.hpp"
#include "message.hpp"
#include "const_buffers_holder.hpp"
#include "common/string.hpp"
#include "error.hpp"

namespace moon
{
    class base_connection :public std::enable_shared_from_this<base_connection>
    {
    public:
        enum class role
        {
            none,
            client,
            server
        };

        using socket_t = asio::ip::tcp::socket;

        template <typename... Args>
        explicit base_connection(uint32_t serviceid, uint8_t type, moon::socket_server* s, Args&&... args)
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

        virtual void start(role r)
        {
            role_ = r;
            recvtime_ = now();
        }

        virtual void read(size_t, std::string_view, int32_t)
        {
            CONSOLE_ERROR(logger(), "Unsupported read operation for PTYPE %d", (int)type_);
            asio::post(socket_.get_executor(), [this, self = shared_from_this()] {
                error(make_error_code(error::invalid_read_operation));
            });
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

            if (wq_warn_size_ != 0 && queue_.size() >= wq_warn_size_)
            {
                CONSOLE_WARN(logger(), "network send queue too long. size:%zu", queue_.size());
                if (wq_error_size_ != 0 && queue_.size() >= wq_error_size_)
                {
                    asio::post(socket_.get_executor(), [this, self = shared_from_this()]() {
                        error(make_error_code(moon::error::send_queue_too_big));
                    });
                    return false;
                }
            }

            bool write_in_progress = !queue_.empty();
            queue_.emplace_back(std::move(data));

            if(!write_in_progress)
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

        uint8_t type() const
        {
            return type_;
        }

        uint32_t owner() const
        {
            return serviceid_;
        }

        role get_role() const
        {
            return role_;
        }

        void timeout(time_t now)
        {
            if ((0 != timeout_) && (0 != recvtime_) && (now - recvtime_ > timeout_))
            {
                asio::post(socket_.get_executor(), [this, self = shared_from_this()]() {
                    error(make_error_code(moon::error::read_timeout));
                });
                return;
            }
            return;
        }

        bool set_no_delay()
        {
            asio::ip::tcp::no_delay option(true);
            asio::error_code ec;
            socket_.set_option(option, ec);
            return !ec;
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
            recvtime_ = now();
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

            asio::async_write(
                socket_,
                make_buffers_ref(holder_.buffers()),
                [this, self = shared_from_this()](const asio::error_code& e, std::size_t)
            {
                if (!e)
                {
                    if (holder_.close())
                    {
                        if (parent_ != nullptr)
                        {
                            parent_->close(fd_);
                            parent_ = nullptr;
                        }
                    }
                    else
                    {
                        for (size_t i = 0; i < holder_.count(); ++i)
                        {
                            queue_.pop_front();
                        }

                        holder_.clear();

                        if(!queue_.empty())
                        {
                            post_send();
                        }
                    }
                }
                else
                {
                    error(e);
                }
            });
        }

        virtual void error(const asio::error_code& e, const std::string& additional = "")
        {
            if (nullptr == parent_)
            {
                return;
            }

            auto msg = message{};
            std::string message = e.message();
            if (!additional.empty())
            {
                message.append("(");
                message.append(additional);
                message.append(")");
            }
            std::string content =
                moon::format("{\"addr\":\"%s\",\"code\":%d,\"message\":\"%s\"}", address().data(), e.value(), message.data());
            msg.write_data(content);

            msg.set_receiver(static_cast<uint8_t>(socket_data_type::socket_close));
            msg.set_sender(fd_);
            parent_->close(fd_);
            handle_message(std::move(msg));
            parent_ = nullptr;
        }

        template<typename Message>
        void handle_message(Message&& m)
        {
            recvtime_ = now();
            if (nullptr != parent_)
            {
                m.set_sender(fd_);
                if (m.type() == 0)
                {
                    m.set_type(type_);
                }
                parent_->handle_message(serviceid_, std::forward<Message>(m));
            }
        }
    protected:
        role role_ = role::none;
        uint32_t fd_ = 0;
        time_t recvtime_ = 0;
        uint32_t timeout_ = 0;
        moon::log* log_ = nullptr;
        uint32_t wq_warn_size_ = 0;
        uint32_t wq_error_size_ = 0;
        uint32_t serviceid_;
        uint8_t type_;
        moon::socket_server* parent_;
        socket_t socket_;
        const_buffers_holder holder_;
        std::deque<buffer_ptr_t> queue_;
    };
}