#pragma once
#include "base_connection.hpp"
#include "common/byte_convert.hpp"
#include "streambuf.hpp"

namespace moon
{
    class moon_connection : public base_connection
    {
    public:
        static constexpr message_size_t MESSAGE_CONTINUED_FLAG = std::numeric_limits<message_size_t>::max();

        using base_connection_t = base_connection;

        template <typename... Args, std::enable_if_t<!std::disjunction_v<std::is_same<std::decay_t<Args>, moon_connection>...>, int> = 0>
        explicit moon_connection(Args&&... args)
            :base_connection(std::forward<Args>(args)...)
            , flag_(enable_chunked::none)
            , cache_(512)
        {
        }

        void start(role r) override
        {
            base_connection_t::start(r);
            message m{};
            m.set_type(type_);
            m.write_data(address());
            m.set_receiver(static_cast<uint8_t>(r == role::server ?
                socket_data_type::socket_accept : socket_data_type::socket_connect));
            handle_message(std::move(m));
            read_header();
        }

        bool send(buffer_shr_ptr_t&& data, socket_send_mask mask) override
        {
            if (data->size() >= MESSAGE_CONTINUED_FLAG && !enum_has_any_bitmask(flag_, enable_chunked::send))
            {
                asio::post(socket_.get_executor(), [this, self = shared_from_this()]() {
                    error(make_error_code(moon::error::write_message_too_big));
                    });
                return false;
            }
            return base_connection_t::send(std::move(data), mask);
        }

        void set_enable_chunked(enable_chunked v)
        {
            flag_ = v;
        }
    private:
        void prepare_send(size_t default_once_send_bytes) override
        {
            size_t bytes = 0;
            size_t queue_size = queue_.size();
            for (size_t i = 0; i < queue_size; ++i) {
                const auto& buf = queue_[i];
                size_t total = buf->size();
                bytes += total;
                const char* data = buf->data();
                wbuffers_.begin_write_slice();
                message_size_t size = 0, header = 0;
                do
                {
                    header = size = (total >= MESSAGE_CONTINUED_FLAG) ? MESSAGE_CONTINUED_FLAG : static_cast<message_size_t>(total);
                    host2net(header);
                    wbuffers_.write_slice(&header, sizeof(header), data, size);
                    total -= size;
                    data += size;
                } while (total != 0);

                if (size == MESSAGE_CONTINUED_FLAG) {
                    header = 0;
                    wbuffers_.write_slice(&header, sizeof(header), nullptr, 0);//end flag
                }

                if (bytes >= default_once_send_bytes){
                    break;
                }
            }
        }

        void read_header()
        {
            if (cache_.size() >= sizeof(message_size_t))
            {
                hanlde_header();
                return;
            }

            asio::async_read(socket_, moon::streambuf(&cache_, cache_.capacity()), asio::transfer_at_least(sizeof(message_size_t)),
                [this, self = shared_from_this()](const asio::error_code& e, std::size_t)
                {
                    if (!e)
                    {
                        hanlde_header();
                        return;
                    }
                    error(e);
                });
        }

        void hanlde_header()
        {
            message_size_t header = 0;
            cache_.read(&header, 1);
            net2host(header);

            bool fin = (header != MESSAGE_CONTINUED_FLAG);
            if (!fin && !enum_has_any_bitmask(flag_, enable_chunked::receive)) {
                error(make_error_code(moon::error::read_message_too_big));
                return;
            }

            read_body(header, fin);
        }

        void read_body(message_size_t size, bool fin)
        {
            if (nullptr == data_)
            {
                data_ = buffer::make_unique((fin ? size : static_cast<size_t>(5) * size) + BUFFER_OPTION_CHEAP_PREPEND);
                data_->commit(BUFFER_OPTION_CHEAP_PREPEND);
            }

            // Calculate the difference between the cache size and the expected size
            ssize_t diff = static_cast<ssize_t>(cache_.size()) - static_cast<ssize_t>(size);
            // Determine the amount of data to consume from the cache
            // If the cache size is greater than or equal to the expected size, consume the expected size
            // Otherwise, consume the entire cache
            size_t consume_size = (diff >= 0 ? size : cache_.size());
            data_->write_back(cache_.data(), consume_size);
            cache_.consume(consume_size);

            if (diff >=0)
            {
                handle_body(fin);
                return;
            }

            cache_.clear();

            asio::async_read(socket_, moon::streambuf(data_.get()), asio::transfer_exactly(static_cast<size_t>(-diff)),
                [this, self = shared_from_this(), size, fin](const asio::error_code& e, std::size_t)
                {
                    if (!e)
                    {
                        handle_body(fin);
                        return;
                    }
                    error(e);
                });
        }

        void handle_body(bool fin)
        {
            if (fin)
            {
                data_->seek(BUFFER_OPTION_CHEAP_PREPEND);
                auto m = message{ std::move(data_) };
                m.set_type(type_);
                m.set_receiver(static_cast<uint8_t>(socket_data_type::socket_recv));
                handle_message(std::move(m));
            }
            read_header();
        }

    protected:
        enable_chunked flag_;
        buffer cache_;
        buffer_ptr_t data_;
    };
}