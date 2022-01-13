#pragma once
#include "base_connection.hpp"
#include "common/byte_convert.hpp"

namespace moon
{
    class moon_connection : public base_connection
    {
    public:
        static constexpr message_size_t MASK_CONTINUED = 1<<(sizeof(message_size_t)*8-1);
        static constexpr message_size_t MAX_CHUNK_SIZE = MASK_CONTINUED ^ std::numeric_limits<message_size_t>::max();

        using base_connection_t = base_connection;

        template <typename... Args>
        explicit moon_connection(Args&&... args)
            :base_connection(std::forward<Args>(args)...)
            , flag_(enable_chunked::none)
            , header_(0)
        {
        }

        void start(bool accepted) override
        {
            base_connection_t::start(accepted);
            auto m = message{};
            m.write_data(address());
            m.set_receiver(static_cast<uint8_t>(accepted ?
                socket_data_type::socket_accept : socket_data_type::socket_connect));
            handle_message(std::move(m));
            read_header();
        }

        bool send(buffer_ptr_t data) override
        {
            if (!data->has_flag(buffer_flag::pack_size))
            {
                if (data->size() > MAX_CHUNK_SIZE)
                {
                    bool enable = (static_cast<int>(flag_)&static_cast<int>(enable_chunked::send)) != 0;
                    if (!enable)
                    {
                        asio::post(socket_.get_executor() , [this, self= shared_from_this()]() {
                            error(make_error_code(moon::error::write_message_too_big));
                        });
                        return false;
                    }
                    data->set_flag(buffer_flag::chunked);
                }
                else
                {
                    message_size_t size = static_cast<message_size_t>(data->size());
                    host2net(size);
                    [[maybe_unused]]  bool res = data->write_front(&size, 1);
                    MOON_ASSERT(res, "tcp::send write front failed");
                    data->set_flag(buffer_flag::pack_size);
                }
            }
            return base_connection_t::send(std::move(data));
        }

        void set_enable_chunked(enable_chunked v)
        {
            flag_ = v;
        }
    protected:
        void message_slice(const_buffers_holder& holder, const buffer_ptr_t& buf) override
        {
            size_t n = buf->size();
            holder.push();
            do
            {
                message_size_t  size = 0, header = 0;
                if (n > MAX_CHUNK_SIZE)
                {
                    size = MAX_CHUNK_SIZE;
                    header = (MASK_CONTINUED | MAX_CHUNK_SIZE);
                }
                else
                {
                    header = size = static_cast<message_size_t>(n);
                }
                const char* data = buf->data() + (buf->size() - n);
                n -= size;
                host2net(header);
                holder.push_slice(header, data, size);
            } while (n != 0);
        }

        void read_header()
        {
            asio::async_read(socket_, asio::buffer(&header_, sizeof(header_)),
                    [this, self = shared_from_this()](const asio::error_code& e, std::size_t bytes_transferred)
            {
                if (e)
                {
                    error(e);
                    return;
                }

                if (bytes_transferred == 0)
                {
                    read_header();
                    return;
                }

                recvtime_ = now();
                net2host(header_);

                bool enable = (static_cast<int>(flag_)&static_cast<int>(enable_chunked::receive)) != 0;
                bool fin = true;
                if (enable)
                {
                    //check is continued message
                    fin = ((header_ & MASK_CONTINUED) == 0);
                    if (!fin)
                    {
                        header_ &= MAX_CHUNK_SIZE;
                    }
                }

                if (header_ > MAX_CHUNK_SIZE)
                {
                    error(make_error_code(moon::error::read_message_too_big));
                    return;
                }
                read_body(header_, fin);
            });
        }

        void read_body(message_size_t size, bool fin)
        {
            if (nullptr == buf_)
            {
                buf_ = message::create_buffer(fin ? size: 5 * size);
            }
            else
            {
                buf_->prepare(size);
            }

            asio::async_read(socket_, asio::buffer(&(*buf_->end()), size),
                    [this, self = shared_from_this(), fin](const asio::error_code& e, std::size_t bytes_transferred)
            {
                if (e)
                {
                    error(e);
                    return;
                }

                if (bytes_transferred == 0)
                {
                    read_header();
                    return;
                }

                buf_->commit(static_cast<int>(bytes_transferred));
                if (fin)
                {
                    auto m = message{std::move(buf_)};
                    m.set_receiver(static_cast<uint8_t>(socket_data_type::socket_recv));
                    handle_message(std::move(m));
                }

                read_header();
            });
        }

    protected:
        enable_chunked flag_;
        message_size_t header_;
        buffer_ptr_t buf_;
    };
}