#pragma once
#include "base_connection.hpp"
#include "common/byte_convert.hpp"

namespace moon
{
    class moon_connection : public base_connection
    {
    public:
        using base_connection_t = base_connection;
        using socket_t = base_connection_t;

        explicit moon_connection(asio::io_service& ios, tcp* t)
            :base_connection(ios, t)
            , msg_size_(0)
        {
        }

        void start(bool accepted, int32_t responseid = 0) override
        {
            base_connection_t::start(accepted, responseid);
            auto msg = message::create();
            msg->write_string(remote_addr_);
            msg->set_sender(id_);
            msg->set_subtype(static_cast<uint8_t>(accepted ? socket_data_type::socket_accept : socket_data_type::socket_connect));
            msg->set_type(PTYPE_SOCKET);
            handle_message(msg);
            read_header();
        }

        bool send(const buffer_ptr_t & data) override
        {
            if (!data->check_flag(uint8_t(buffer_flag::pack_size)))
            {
                message_size_t size = static_cast<message_size_t>(data->size());
                host2net(size);
                MOON_DCHECK(data->write_front(&size, 0, 1), "send_packsize write_front failed");
                data->set_flag(uint8_t(buffer_flag::pack_size));
            }
            return base_connection_t::send(data);
        }

    protected:
        void read_header()
        {
            asio::async_read(socket_, asio::buffer(&msg_size_, sizeof(msg_size_)),
                make_custom_alloc_handler(allocator_,
                    [this, self = shared_from_this()](const asio::error_code& e, std::size_t bytes_transferred)
            {
                if (e)
                {
                    error(e, int(network_logic_error::ok));
                    return;
                }

                if (bytes_transferred == 0)
                {
                    read_header();
                    return;
                }

                last_recv_time_ = std::time(nullptr);
                net2host(msg_size_);
                if (msg_size_ > MAX_NMSG_SIZE)
                {
                    error(asio::error_code(), int(network_logic_error::read_message_size_max));
                    close();
                    return;
                }
                read_body(msg_size_);
            }));
        }

        void read_body(message_size_t size)
        {
            auto buf = message::create_buffer(size);
            asio::async_read(socket_, asio::buffer((void*)buf->data(), size),
                make_custom_alloc_handler(allocator_,
                    [this, self = shared_from_this(), buf](const asio::error_code& e, std::size_t bytes_transferred)
            {
                if (e)
                {
                    error(e, int(network_logic_error::ok));
                    return;
                }

                if (bytes_transferred == 0)
                {
                    read_header();
                    return;
                }

                buf->offset_writepos(static_cast<int>(bytes_transferred));
                auto msg = message::create(buf);
                msg->set_sender(id_);
                msg->set_subtype(static_cast<uint8_t>(socket_data_type::socket_recv));
                msg->set_type(PTYPE_SOCKET);
                handle_message(msg);

                read_header();
            }));
        }

    protected:
        message_size_t msg_size_;
    };
}