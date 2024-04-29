#pragma once
#include "config.hpp"
#include "asio.hpp"
#include "message.hpp"
#include "common/vec_deque.hpp"
#include "common/string.hpp"
#include "write_buffer.hpp"
#include "error.hpp"

namespace moon
{
    class base_connection :public std::enable_shared_from_this<base_connection>
    {
    public:
        enum class role: uint8_t
        {
            none,
            client,
            server
        };

        using socket_t = asio::ip::tcp::socket;

        template <typename... Args>
        explicit base_connection(uint32_t serviceid, uint8_t type, moon::socket_server* s, Args&&... args)
            : type_(type)
            , serviceid_(serviceid)
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

        virtual std::optional<std::string_view> read(size_t, std::string_view, int64_t)
        {
            CONSOLE_ERROR("Unsupported read operation for PTYPE %d", (int)type_);
            asio::post(socket_.get_executor(), [this, self = shared_from_this()] {
                error(make_error_code(error::invalid_read_operation));
                });
            return std::nullopt;
        };

        virtual bool send(buffer_shr_ptr_t&& data, socket_send_mask mask)
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
                CONSOLE_WARN("network send queue too long. size:%zu", queue_.size());
                if (wq_error_size_ != 0 && queue_.size() >= wq_error_size_)
                {
                    asio::post(socket_.get_executor(), [this, self = shared_from_this()]() {
                        error(make_error_code(moon::error::send_queue_too_big));
                        });
                    return false;
                }
            }

            will_close_ =  enum_has_any_bitmask(mask, socket_send_mask::close) ? true : will_close_;

            queue_.push_back(std::move(data));

            if (queue_.size() == 1)
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

        void settimeout(uint32_t v)
        {
            timeout_ = v;
            recvtime_ = now();
        }

        void set_send_queue_limit(uint16_t warnsize, uint16_t errorsize)
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
        virtual void prepare_send(size_t default_once_send_bytes)
        {
            size_t bytes = 0;
            size_t queue_size = queue_.size();
            for (size_t i = 0; i < queue_size; ++i) {
                const auto& buf = queue_[i];
                wbuffers_.write(buf->data(), buf->size());
                bytes+= buf->size();
                if (bytes >= default_once_send_bytes)
                {
                    break;
                }
            }
        }

        void post_send()
        {
            prepare_send(262144);
 
            asio::async_write(
                socket_,
                make_buffers_ref(wbuffers_.buffers()),
                [this, self = shared_from_this()](const asio::error_code& e, std::size_t)
                {
                    if (e)
                    {
                        error(e);
                        return;
                    }

                    for (size_t i = 0; i < wbuffers_.size(); ++i)
                    {
                        queue_.pop_front();
                    }

                    wbuffers_.clear();

                    if (!queue_.empty())
                    {
                        post_send();
                    }
                    else if (will_close_ && parent_ != nullptr)
                    {
                        parent_->close(fd_);
                        parent_ = nullptr;
                    }
                });
        }

        virtual void error(const asio::error_code& e, int64_t session = 0, const std::string& additional = "")
        {
            (void)session;
            if (nullptr == parent_){
                return;
            }
            std::string str = e.message();
            if (!additional.empty())
            {
                str.append("(");
                str.append(additional);
                str.append(")");
            }
            std::string content =
                moon::format("{\"addr\":\"%s\",\"code\":%d,\"message\":\"%s\"}", address().data(), e.value(), str.data());
            
            message msg{};
            msg.set_type(type_);
            msg.write_data(content);
            msg.set_receiver(static_cast<uint8_t>(socket_data_type::socket_close));

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
                parent_->handle_message(serviceid_, std::forward<Message>(m));
            }
        }
    protected:
        bool will_close_ = false;
        bool read_in_progress_ = false;
        role role_ = role::none;
        uint8_t type_ = 0;
        uint16_t wq_warn_size_ = 0;
        uint16_t wq_error_size_ = 0;
        uint32_t fd_ = 0;
        uint32_t timeout_ = 0;
        uint32_t serviceid_ = 0;
        time_t recvtime_ = 0;
        moon::socket_server* parent_;
        write_buffer wbuffers_;
        VecDeque<buffer_shr_ptr_t> queue_;
        socket_t socket_;
    };
}