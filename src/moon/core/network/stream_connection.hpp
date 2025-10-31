#pragma once
#include "base_connection.hpp"
#include "common/static_string.hpp"
#include "streambuf.hpp"

namespace moon {
struct read_until {
    static constexpr size_t max_delim_size = 7;
    size_t max_size = 0;
    static_string<max_delim_size> delim;

    read_until(size_t max_size, std::string_view delims):
        max_size(max_size > 0 ? max_size : std::numeric_limits<size_t>::max()),
        delim(delims) {}
};

struct read_exactly {
    size_t size;
};

class stream_connection: public base_connection {
public:
    using base_connection_t = base_connection;

    static constexpr size_t DEFAULT_READ_CACHE_SIZE = 8192;
    static constexpr size_t DOUBLE_CHAR_DELIM_SIZE = 2;

    template<
        typename... Args,
        std::enable_if_t<
            !std::disjunction_v<std::is_same<std::decay_t<Args>, stream_connection>...>,
            int> = 0>
    explicit stream_connection(Args&&... args): base_connection_t(std::forward<Args>(args)...) {}

    direct_read_result read(size_t size, std::string_view delim, int64_t session) override {
        if (!is_open()) {
            CONSOLE_ERROR("Connection closed. fd=%u", fd_);
            return direct_read_result { false, { "Connection closed" } };
        }

        if (enum_has_any_bitmask(mask_, connection_mask::reading)) {
            CONSOLE_ERROR("Already reading. fd=%u", fd_);
            return direct_read_result { false, { "Already reading" } };
        }

        buffer* buf = read_cache_.as_buffer();
        buf->commit_unchecked(std::exchange(more_bytes_, 0));
        buf->consume_unchecked(std::exchange(consume_, 0));

        mask_ = mask_ | connection_mask::reading;
        read_cache_.session = session;

        return delim.empty() ? read(read_exactly { size }) : read(read_until { size, delim });
    }

private:
    static size_t find_delimiter(std::string_view data, std::string_view delim, size_t delim_size) {
        if (delim_size == 1) {
            return data.find(delim[0]);
        } else if (delim_size == DOUBLE_CHAR_DELIM_SIZE) {
            const char first = delim[0];
            const char second = delim[1];
            const size_t max_pos = data.size() - 1;

            for (size_t i = 0; i < max_pos; ++i) {
                if (data[i] == first && data[i + 1] == second) {
                    return i;
                }
            }
            return std::string_view::npos;
        } else {
            return data.find(delim);
        }
    }

    direct_read_result read(const read_until& op) {
        const size_t delim_size = op.delim.size();

        if (read_cache_.size() >= delim_size) {
            std::string_view data { read_cache_.data(), read_cache_.size() };
            const size_t pos = find_delimiter(data, op.delim.to_string_view(), delim_size);

            if (pos != std::string_view::npos) {
                mask_ = enum_unset_bitmask(mask_, connection_mask::reading);
                read_cache_.as_buffer()->consume_unchecked(pos + delim_size);
                return direct_read_result { true, { data.data(), pos } };
            }
        }

        asio::async_read_until(
            socket_,
            moon::streambuf(read_cache_.as_buffer(), op.max_size),
            op.delim.to_string_view(),
            [this,
             self = shared_from_this(),
             delim_size =
                 op.delim.size()](const asio::error_code& e, std::size_t bytes_transferred) {
                if (!e) {
                    response(bytes_transferred, delim_size);
                    return;
                }
                error(e);
            }
        );
        return direct_read_result { true, {} };
    }

    direct_read_result read(read_exactly op) {
        if (read_cache_.size() >= op.size) {
            mask_ = enum_unset_bitmask(mask_, connection_mask::reading);
            consume_ = op.size;
            return direct_read_result { true, { read_cache_.data(), op.size } };
        }

        const std::size_t need_size = op.size - read_cache_.size();
        asio::async_read(
            socket_,
            moon::streambuf { read_cache_.as_buffer(), op.size },
            asio::transfer_exactly(need_size),
            [this,
             self = shared_from_this(),
             size = op.size](const asio::error_code& e, std::size_t) {
                if (!e) {
                    response(size, 0);
                    return;
                }
                error(e);
            }
        );
        return direct_read_result { true, {} };
    }

    void
    error(const asio::error_code& e, [[maybe_unused]] const std::string& additional = "") override {
        if (parent_ == nullptr) {
            return;
        }

        mask_ = enum_unset_bitmask(mask_, connection_mask::reading);

        auto b = read_cache_.as_buffer();
        b->clear();

        if (e) {
            if (e == moon::error::read_timeout) {
                b->write_back(moon::format("TIMEOUT %s.(%d)", e.message().data(), e.value()));
            } else if (e == asio::error::eof) {
                b->write_back(moon::format("EOF %s.(%d)", e.message().data(), e.value()));
            } else {
                b->write_back(moon::format("SOCKET_ERROR %s.(%d)", e.message().data(), e.value()));
            }
        }

        parent_->close(fd_);

        if (enum_has_any_bitmask(mask_, connection_mask::reading)) {
            response(read_cache_.size(), 0, PTYPE_ERROR);
        }
        parent_ = nullptr;
    }

    void response(size_t count, size_t remove_tail, uint8_t type = PTYPE_SOCKET_TCP) {
        if (parent_ == nullptr) {
            return;
        }

        buffer* buf = read_cache_.as_buffer();
        size_t size = buf->size();
        assert(size >= count);

        more_bytes_ = (size - count) + remove_tail;
        consume_ = count;
        buf->revert(more_bytes_);
        read_cache_.type = type;
        read_cache_.sender = fd_;

        mask_ = enum_unset_bitmask(mask_, connection_mask::reading);

        assert(read_cache_.session != 0);
        handle_message(read_cache_);
    }

protected:
    size_t more_bytes_ = 0;
    size_t consume_ = 0;
    message read_cache_ { DEFAULT_READ_CACHE_SIZE };
};
} // namespace moon