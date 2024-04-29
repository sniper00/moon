#pragma once
#include "common/static_string.hpp"
#include "base_connection.hpp"
#include "streambuf.hpp"

namespace moon
{
    struct read_until
    {
        static constexpr size_t max_delim_size = 7;
        size_t max_size = 0;
        int64_t session = 0;
        static_string<max_delim_size> delim;

        read_until() = default;

        read_until(size_t max_size, int64_t session, std::string_view delims)
            : max_size(max_size>0?max_size: std::numeric_limits<size_t>::max())
            , session(session)
            , delim(delims)
        {
 
        }
    };

    struct read_exactly
    {
        size_t size;
        int64_t session;
    };

    class stream_connection : public base_connection
    {
    public:
        using base_connection_t = base_connection;

        template <typename... Args, std::enable_if_t<!std::disjunction_v<std::is_same<std::decay_t<Args>, stream_connection>...>, int> = 0>
        explicit stream_connection(Args&&... args)
            :base_connection_t(std::forward<Args>(args)...)
            ,read_cache_(8192)
        {
        }

        std::optional<std::string_view> read(size_t size, std::string_view delim, int64_t session) override
        {
            if (!is_open() || read_in_progress_)
            {
                CONSOLE_ERROR("invalid read operation. %u", fd_);
                asio::post(socket_.get_executor(), [this, self = shared_from_this(), session]()
                    { error(make_error_code(error::invalid_read_operation), session); });
                return std::nullopt;
            }

            read_in_progress_ = true;

            buffer* buf = read_cache_.as_buffer();
            buf->commit(more_bytes_);
            buf->consume(consume_);
            more_bytes_ = 0;
            consume_ = 0;

            if (!delim.empty()) {
                return read(read_until{size, session, delim });
            }
            else {
                return read(read_exactly{size, session });
            }
        }

    private:
        std::optional<std::string_view> read(read_until op)
        {
            size_t delim_size = op.delim.size();
            if (read_cache_.size() >= delim_size) {
                std::string_view data{ read_cache_.data(), read_cache_.size() };
                std::default_searcher searcher{ op.delim.data(), op.delim.data() + delim_size };
                auto it = std::search(data.begin(), data.end(), searcher);
                if (it != data.end()) {
                    read_in_progress_ = false;
                    auto count = std::distance(data.begin(), it);
                    read_cache_.as_buffer()->consume(count + delim_size);
                    return std::make_optional<std::string_view>(data.data(), count);
                }
            }

            asio::async_read_until(socket_, moon::streambuf(read_cache_.as_buffer(), op.max_size), op.delim.to_string_view(),
                [this, self = shared_from_this(), op](const asio::error_code& e, std::size_t bytes_transferred)
                {
                    if (!e)
                    {
                        response(op.session, bytes_transferred, op.delim.size());
                        return;
                    }
                    error(e, op.session);
                });
            return std::nullopt;
        }

        std::optional<std::string_view> read(read_exactly op)
        {
            if (read_cache_.size() >= op.size)
            {
                read_in_progress_ = false;
                consume_ = op.size;
                return std::make_optional<std::string_view>(read_cache_.data(), op.size);
            };

            std::size_t size = op.size - read_cache_.size();
            asio::async_read(socket_, moon::streambuf(read_cache_.as_buffer(), op.size), asio::transfer_exactly(size),
                [this, self = shared_from_this(), op](const asio::error_code& e, std::size_t)
                {
                    if (!e)
                    {
                        response(op.session, op.size, 0);
                        return;
                    }
                    error(e, op.session);
                });
            return std::nullopt;
        }

        void error(const asio::error_code& e, int64_t session, const std::string& additional = "") override
        {
            (void)additional;

            if(parent_ == nullptr){
                return;
            }

            read_cache_.as_buffer()->clear();

            if (e)
            {
                if (e == moon::error::read_timeout)
                {
                    read_cache_.write_data(moon::format("TIMEOUT %s.(%d)", e.message().data(), e.value()));
                }
                else if (e == asio::error::eof)
                {
                    read_cache_.write_data(moon::format("EOF %s.(%d)", e.message().data(), e.value()));
                }
                else
                {
                    read_cache_.write_data(moon::format("SOCKET_ERROR %s.(%d)", e.message().data(), e.value()));
                }
            }

            parent_->close(fd_);
            response(session, read_cache_.size(), 0, PTYPE_ERROR);
            parent_ = nullptr;
        }

        void response(int64_t session, size_t count, size_t remove_tail, uint8_t type = PTYPE_SOCKET_TCP)
        {
            if(parent_ == nullptr){
                return;
            }

            buffer* buf = read_cache_.as_buffer();
            size_t size = buf->size();
            assert(size >= count);

            more_bytes_ = (size - count) + remove_tail;
            consume_ = count;
            buf->revert(more_bytes_);
            read_cache_.set_type(type);
            read_cache_.set_sender(fd_);
            read_cache_.set_sessionid(session);

            read_in_progress_ = false;

            assert(session != 0);
            handle_message(read_cache_);
        }
    protected:
        size_t more_bytes_ = 0;
        size_t consume_ = 0;
        message read_cache_;
    };
}