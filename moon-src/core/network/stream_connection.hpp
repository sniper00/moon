#pragma once
#include "base_connection.hpp"
#include "streambuf.hpp"

namespace moon
{
    class stream_connection : public base_connection
    {
    public:
        using base_connection_t = base_connection;

        template <typename... Args, std::enable_if_t<!std::disjunction_v<std::is_same<std::decay_t<Args>, stream_connection>...>, int> = 0>
        explicit stream_connection(Args&&... args)
            :base_connection_t(std::forward<Args>(args)...)
            ,response_(8192,0)
        {
        }

        void read(size_t n, std::string_view delim, int64_t sessionid) override
        {
            if (!is_open() || sessionid_ != 0)
            {
                CONSOLE_ERROR("invalid read operation. %u", fd_);
                asio::post(socket_.get_executor(), [this, self = shared_from_this()]() {
                    error(make_error_code(error::invalid_read_operation));
                });
                return;
            }

            buffer* buf = response_.as_buffer();
            std::size_t size = buf->size() + delim_.size();
            buf->commit(revert_);
            buf->consume(size);
            revert_ = 0;

            delim_ = delim;
            sessionid_ = sessionid;

            if (!delim_.empty())
            {
                read_until((n > 0 ? n : std::numeric_limits<size_t>::max()));
            }
            else
            {
                read(n);
            }
        }

    protected:
        void read_until(size_t count)
        {
            asio::async_read_until(socket_, moon::streambuf(response_.as_buffer(), count), delim_,
                    [this, self = shared_from_this()](const asio::error_code& e, std::size_t bytes_transferred)
            {
                if (e)
                {
                    error(e);
                    return;
                }
                response(bytes_transferred);
            });
        }

        void read(size_t count)
        {
            std::size_t size = (response_.size() >= count ? 0 : (count - response_.size()));
            asio::async_read(socket_, moon::streambuf(response_.as_buffer(), count), asio::transfer_exactly(size),
                    [this, self = shared_from_this(), count](const asio::error_code& e, std::size_t)
            {
                if (e)
                {
                    error(e);
                    return;
                }
                response(count);
            });
        }

        void error(const asio::error_code& e, const std::string& additional = "") override
        {
            (void)additional;

            if (nullptr == parent_)
            {
                return;
            }
            delim_.clear();
            response_.as_buffer()->clear();

            if (e)
            {
                if (e == moon::error::read_timeout)
                {
                    response_.write_data(moon::format("TIMEOUT %s.(%d)", e.message().data(), e.value()));
                }
                else if (e == asio::error::eof)
                {
                    response_.write_data(moon::format("EOF %s.(%d)", e.message().data(), e.value()));
                }
                else
                {
                    response_.write_data(moon::format("SOCKET_ERROR %s.(%d)", e.message().data(), e.value()));
                }
            }

            parent_->close(fd_);

            if (sessionid_ != 0)
            {
                response(0, PTYPE_ERROR);
            }
            parent_ = nullptr;
        }

        void response(size_t count, uint8_t type = PTYPE_SOCKET_TCP)
        {
            if (sessionid_ == 0)
            {
                return;
            }
            auto buf = response_.as_buffer();
            assert(buf->size() >= count);
            revert_ = 0;
            if (type == PTYPE_SOCKET_TCP)
            {
                revert_ = (buf->size() - count) + delim_.size();
            }   
            buf->revert(revert_);
            response_.set_type(type);
            response_.set_sender(fd());
            response_.set_sessionid(sessionid_);
            sessionid_ = 0;
            handle_message(response_);
        }
    protected:
        size_t revert_ = 0;
        int64_t sessionid_ = 0;
        std::string delim_;
        message response_;
    };
}