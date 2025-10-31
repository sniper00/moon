#pragma once
#include "base_connection.hpp"
#include "common/byte_convert.hpp"
#include "streambuf.hpp"

namespace moon {
class moon_connection: public base_connection {
public:
    static constexpr message_size_t MESSAGE_CONTINUED_FLAG =
        std::numeric_limits<message_size_t>::max();

    static constexpr size_t DEFAULT_READ_CACHE_SIZE = 512;

    using base_connection_t = base_connection;

    template<
        typename... Args,
        std::enable_if_t<
            !std::disjunction_v<std::is_same<std::decay_t<Args>, moon_connection>...>,
            int> = 0>
    explicit moon_connection(Args&&... args): base_connection(std::forward<Args>(args)...) {}

    void start(bool server) override {
        base_connection_t::start(server);
        handle_message(
            message {
                type_,
                0,
                static_cast<uint8_t>(
                    server ? socket_data_type::socket_accept : socket_data_type::socket_connect
                ),
                0,
                address() }
        );
        read_header();
    }

    bool send(buffer_shr_ptr_t data) override {
        if (data->size() >= MESSAGE_CONTINUED_FLAG
            && !enum_has_any_bitmask(mask_, connection_mask::chunked_send))
        {
            asio::post(socket_.get_executor(), [this, self = shared_from_this()]() {
                error(make_error_code(moon::error::write_message_too_big));
            });
            return false;
        }
        return base_connection_t::send(std::move(data));
    }

    void set_enable_chunked(connection_mask v) {
        // Clear both chunked flags first
        mask_ = enum_unset_bitmask(
            mask_,
            connection_mask::chunked_send | connection_mask::chunked_recv
        );
        // Set the requested flags
        mask_ = mask_ | (v & (connection_mask::chunked_send | connection_mask::chunked_recv));
    }

private:
    void prepare_send(size_t default_once_send_bytes) override {
        wqueue_.prepare_buffers(
            [this](const buffer_shr_ptr_t& elm) {
                size_t size = elm->size();
                const char* data = elm->data();
                if (!elm->has_bitmask(socket_send_mask::raw)) {
                    wqueue_.consume();
                    message_size_t slice_size = 0;
                    message_size_t header = 0;
                    do {
                        header = slice_size = (size >= MESSAGE_CONTINUED_FLAG)
                            ? MESSAGE_CONTINUED_FLAG
                            : static_cast<message_size_t>(size);
                        host2net(header);
                        wqueue_.prepare_with_padding(&header, sizeof(header), data, slice_size);
                        size -= slice_size;
                        data += slice_size;
                    } while (size != 0);

                    if (slice_size == MESSAGE_CONTINUED_FLAG) {
                        header = 0;
                        wqueue_
                            .prepare_with_padding(&header, sizeof(header), nullptr, 0); //end flag
                    }
                } else {
                    wqueue_.consume(data, size);
                }
            },
            default_once_send_bytes
        );
    }

    void read_header() {
        if (cache_.size() >= sizeof(message_size_t)) {
            handle_header();
            return;
        }

        asio::async_read(
            socket_,
            moon::streambuf(&cache_, cache_.capacity()),
            asio::transfer_at_least(sizeof(message_size_t)),
            [this, self = shared_from_this()](const asio::error_code& e, std::size_t) {
                if (!e) {
                    handle_header();
                    return;
                }
                error(e);
            }
        );
    }

    void handle_header() {
        message_size_t header = 0;
        [[maybe_unused]] bool always_ok = cache_.read(&header, 1);
        net2host(header);

        bool fin = (header != MESSAGE_CONTINUED_FLAG);
        if (!fin && !enum_has_any_bitmask(mask_, connection_mask::chunked_recv)) {
            error(make_error_code(moon::error::read_message_too_big));
            return;
        }

        read_body(header, fin);
    }

    void read_body(message_size_t size, bool fin) {
        if (nullptr == data_) {
            // More conservative memory allocation for chunked messages
            // Allocate exact size for final chunks, or use a growth factor for continued chunks
            size_t alloc_size = fin ? size : (static_cast<size_t>(size) * 2);
            data_ = buffer::make_unique(alloc_size + BUFFER_OPTION_CHEAP_PREPEND);
            data_->commit_unchecked(BUFFER_OPTION_CHEAP_PREPEND);
        }

        // Calculate the difference between the cache size and the expected size
        ssize_t diff = static_cast<ssize_t>(cache_.size()) - static_cast<ssize_t>(size);
        // Determine the amount of data to consume from the cache
        size_t consume_size = (diff >= 0) ? size : cache_.size();

        data_->write_back({ cache_.data(), consume_size });
        cache_.consume_unchecked(consume_size);

        if (diff >= 0) {
            handle_body(fin);
            return;
        }

        cache_.clear();

        asio::async_read(
            socket_,
            moon::streambuf(data_.get()),
            asio::transfer_exactly(static_cast<size_t>(-diff)),
            [this, self = shared_from_this(), fin](const asio::error_code& e, std::size_t) {
                if (!e) {
                    handle_body(fin);
                    return;
                }
                error(e);
            }
        );
    }

    void handle_body(bool fin) {
        if (fin) {
            data_->consume_unchecked(BUFFER_OPTION_CHEAP_PREPEND);
            handle_message(
                message { type_,
                          0,
                          static_cast<uint8_t>(socket_data_type::socket_recv),
                          0,
                          std::move(data_) }
            );
        }
        read_header();
    }

private:
    buffer cache_ { DEFAULT_READ_CACHE_SIZE };
    buffer_ptr_t data_;
};
} // namespace moon