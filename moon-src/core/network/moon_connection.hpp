#pragma once
#include "base_connection.hpp"
#include "common/byte_convert.hpp"

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
            , header_(0)
        {
        }

        void start(role r) override
        {
            base_connection_t::start(r);
            auto addr = address();
            message m{};
            m.write_data(address());
            m.set_receiver(static_cast<uint8_t>(r == role::server ?
                socket_data_type::socket_accept : socket_data_type::socket_connect));
            handle_message(std::move(m));
            read_header();
        }

        bool send(buffer_shr_ptr_t&& data) override
        {
            if (!data->has_flag(buffer_flag::pack_size))
            {
                if (data->size() >= MESSAGE_CONTINUED_FLAG)
                {
                    if (!enum_has_any_bitmask(flag_, enable_chunked::send))
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
        void message_slice(const_buffers_holder& holder, const buffer_shr_ptr_t& buf) override
        {
            size_t total = buf->size();
            const char* p = buf->data();
            holder.push();
            message_size_t  size = 0, header = 0;
            do
            {
                header = size = (total >= MESSAGE_CONTINUED_FLAG) ? MESSAGE_CONTINUED_FLAG : static_cast<message_size_t>(total);
                host2net(header);
                holder.push_slice(header, p, size);
                total -= size;
                p += size;
            } while (total != 0);
            if(size == MESSAGE_CONTINUED_FLAG)
                holder.push_slice(0, nullptr, 0);//end flag
        }

        void read_header()
        {
            header_ = 0;
            asio::async_read(socket_, asio::buffer(&header_, sizeof(header_)),
                    [this, self = shared_from_this()](const asio::error_code& e, std::size_t)
            {
                if (e)
                {
                    error(e);
                    return;
                }

                net2host(header_);

                bool fin = (header_ != MESSAGE_CONTINUED_FLAG);
                if (!fin && !enum_has_any_bitmask(flag_, enable_chunked::receive)) {
                    error(make_error_code(moon::error::read_message_too_big));
                    return;
                }

                read_body(header_, fin);
            });
        }

        void read_body(message_size_t size, bool fin)
        {
            if (nullptr == buf_)
                buf_ = buffer::make_unique(fin ? size : static_cast<size_t>(5) * size);

            auto space = buf_->prepare(size);

            asio::async_read(socket_, asio::buffer(space.first, space.second),
                    [this, self = shared_from_this(), fin](const asio::error_code& e, std::size_t bytes_transferred)
            {
                if (e)
                {
                    error(e);
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