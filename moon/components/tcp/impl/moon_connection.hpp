#pragma once
#include "base_connection.hpp"
#include "common/byte_convert.hpp"

namespace moon
{
    class moon_connection : public base_connection
    {
    public:
        static constexpr message_size_t INCOMPLETE_FLAG = 0x8000;
        static constexpr message_size_t MAX_MSG_FRAME_SIZE = MAX_NET_MSG_SIZE - sizeof(message_size_t);

        using base_connection_t = base_connection;

        template <typename... Args>
        explicit moon_connection(Args&&... args)
            :base_connection(std::forward<Args>(args)...)
            , continue_(false)
            , header_(0)
        {
        }

        void start(bool accepted, int32_t responseid = 0) override
        {
            base_connection_t::start(accepted, responseid);
            auto msg = message::create();
            msg->write_string(remote_addr_);
            msg->set_sender(id_);
            msg->set_subtype(static_cast<uint8_t>(accepted ? socket_data_type::socket_accept : socket_data_type::socket_connect));
            handle_message(std::move(msg));
            read_header();
        }

        bool send(const buffer_ptr_t & data) override
        {
            if (!data->has_flag(buffer_flag::pack_size))
            {
                if (data->size() > MAX_MSG_FRAME_SIZE)
                {
                    bool enable = (static_cast<int>(frame_flag_)&static_cast<int>(frame_enable_flag::send)) != 0;
                    if (!enable)
                    {
                        error(asio::error_code(), int(network_logic_error::send_message_size_max));
                        base_connection_t::close();
                        return false;
                    }
                    data->set_flag(buffer_flag::framing);
                }
                else
                {
                    message_size_t size = static_cast<message_size_t>(data->size());
                    host2net(size);
                    bool res = data->write_front(&size, 0, 1);
                    MOON_DCHECK(res, "tcp::send write front failed");
                    if (res)
                    {
                        data->set_flag(buffer_flag::pack_size);
                    }
                }
            }
            return base_connection_t::send(data);
        }

    protected:
        void message_framing(const_buffers_holder& holder, const buffer_ptr_t& buf) override
        {
            size_t n = buf->size();
            holder.framing_begin(buf, n / MAX_NET_MSG_SIZE + 1);
            do
            {
                message_size_t  size = 0, header = 0;
                if (n > MAX_NET_MSG_SIZE)
                {
                    header = size = MAX_NET_MSG_SIZE;
                    header |= INCOMPLETE_FLAG;
                }
                else
                {
                    header = size = static_cast<message_size_t>(n);
                }
                const char* data = buf->data() + (buf->size() - n);
                n -= size;
                host2net(header);
                holder.push_framing(header, data, size);
            } while (n != 0);
        }

        void read_header()
        {
            asio::async_read(socket_, asio::buffer(&header_, sizeof(header_)),
                make_custom_alloc_handler(read_allocator_,
                    [this, self = shared_from_this()](const asio::error_code& e, std::size_t bytes_transferred)
            {
                if (e)
                {
                    error(e, int(logic_error_));
                    return;
                }

                if (bytes_transferred == 0)
                {
                    read_header();
                    return;
                }

                last_recv_time_ = std::time(nullptr);
                net2host(header_);

                bool enable = (static_cast<int>(frame_flag_)&static_cast<int>(frame_enable_flag::receive)) != 0;
                if (enable)
                {
                    //check is continue message
                    continue_ = ((header_ & INCOMPLETE_FLAG) != 0);
                    if (continue_)
                    {
                        header_ &= (~INCOMPLETE_FLAG);
                    }
                }

                if (header_ > MAX_NET_MSG_SIZE)
                {
                    error(asio::error_code(), int(network_logic_error::read_message_size_max));
                    base_connection_t::close();
                    return;
                }
                read_body(header_);
            }));
        }

        void read_body(message_size_t size)
        {
            if (nullptr == buf_)
            {
                buf_ = message::create_buffer(continue_ ? 5 * size : size);
            }
            else
            {
                buf_->check_space(size);
            }

            asio::async_read(socket_, asio::buffer((buf_->data() + buf_->size()), size),
                make_custom_alloc_handler(read_allocator_,
                    [this, self = shared_from_this()](const asio::error_code& e, std::size_t bytes_transferred)
            {
                if (e)
                {
                    error(e, int(logic_error_));
                    return;
                }

                if (bytes_transferred == 0)
                {
                    read_header();
                    return;
                }

                buf_->offset_writepos(static_cast<int>(bytes_transferred));
                if (!continue_)
                {
                    auto msg = message::create(buf_);
                    buf_.reset();
                    msg->set_sender(id_);
                    msg->set_subtype(static_cast<uint8_t>(socket_data_type::socket_recv));
                    handle_message(std::move(msg));
                }
                read_header();
            }));
        }

    protected:
        bool continue_;
        message_size_t header_;
        buffer_ptr_t buf_;
    };
}