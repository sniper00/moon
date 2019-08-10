#pragma once
#include "base_connection.hpp"

namespace moon
{
    class custom_connection : public base_connection
    {
    public:
        using base_connection_t = base_connection;

        class scope_buffer_offset
        {
            int offset_rpos_ = 0;
            int offset_wpos_ = 0;
            buffer* buf_ = nullptr;
        public:
            scope_buffer_offset(buffer* buf, int offset_rpos, int offset_wpos)
                :offset_rpos_(offset_rpos)
                , offset_wpos_(offset_wpos)
                , buf_(buf)
            {
                buf->offset_writepos(offset_wpos_);
            }

            ~scope_buffer_offset()
            {
                buf_->offset_writepos(-offset_wpos_);
                buf_->seek(offset_rpos_, buffer::Current);
            }
        };

        template <typename... Args>
        explicit custom_connection(Args&&... args)
            :base_connection_t(std::forward<Args>(args)...)
        {
        }

        void start(bool accepted) override
        {
            base_connection_t::start(accepted);
            response_ = message::create(8192);
            read_some();
        }

        bool read(const read_request& ctx) override
        {
            if (is_open() && request_.sessionid == 0)
            {
                request_ = ctx;
                if (response_->size() > 0)
                {
                    //guarantee read is async operation
                    asio::post(socket_.get_executor(),[this, self = shared_from_this()] {
                        if (socket_.is_open())
                        {
                            handle_read_request();
                        }
                    });
                }
                return true;
            }
            return false;
        }

    protected:
        void read_some()
        {
            auto buf = response_->get_buffer();
            buf->check_space(8192);
            socket_.async_read_some(asio::buffer((buf->data() + buf->size()), buf->writeablesize()),
                make_custom_alloc_handler(rallocator_,
                    [this, self = shared_from_this()](const asio::error_code& e, std::size_t bytes_transferred)
            {
                if (e)
                {
                    error(e);
                    return;
                }

                if (bytes_transferred == 0)
                {
                    read_some();
                    return;
                }

                //CONSOLE_DEBUG(logger(), "connection recv:%u %s",id_, moon::to_hex_string(string_view_t{ (char*)buffer_.data(), bytes_transferred }, " ").data(), "");

                recvtime_ = now();
                response_->get_buffer()->offset_writepos(static_cast<int>(bytes_transferred));
                handle_read_request();
                read_some();
            }));
        }

        void read_with_delim(buffer* buf, const string_view_t& delim)
        {
            size_t dszie = buf->size();
            if (request_.size != 0 && dszie > request_.size)
            {
                error(make_error_code(moon::error::read_message_too_big));
                return;
            }

            string_view_t strref(buf->data(), dszie);
            size_t pos = strref.find(delim);
            if (pos != string_view_t::npos)
            {
                scope_buffer_offset sbo{ buf , static_cast<int>(pos + delim.size()) , -static_cast<int>(dszie - pos) };
                response(response_);
            }
        }

        void handle_read_request()
        {
            auto buf = response_->get_buffer();
            size_t dszie = buf->size();

            if (0 == request_.sessionid || dszie == 0)
            {
                return;
            }

            switch (request_.delim)
            {
            case read_delim::FIXEDLEN:
            {
                if (buf->size() >= request_.size)
                {
                    scope_buffer_offset sbo{ buf ,  static_cast<int>(request_.size) , -static_cast<int>(dszie - request_.size) };
                    response(response_);
                }
                break;
            }
            case read_delim::LF:
            {
                read_with_delim(buf, STR_LF);
                break;
            }
            case read_delim::CRLF:
            {
                read_with_delim(buf, STR_CRLF);
                break;
            }
            case read_delim::DCRLF:
            {
                read_with_delim(buf, STR_DCRLF);
                break;
            }
            default:
                break;
            }
        }

        void error(const asio::error_code& e)
        {
            s_->close(fd_, true);

            response_->get_buffer()->clear();

            if (e && e != asio::error::eof)
            {
                response_->set_header("closed");
                response_->write_data(moon::format("%s.(%d)", e.message().data(), e.value()));
            }

            if (request_.sessionid != 0)
            {
                response(response_, PTYPE_ERROR);
            }
        }

        void response(const message_ptr_t & m, uint8_t type = PTYPE_TEXT)
        {
            m->set_type(type);
            m->set_sender(fd());
            m->set_sessionid(request_.sessionid);
            request_.sessionid = 0;
            request_.size = 0;
            handle_message(m);
        }
    protected:
        message_ptr_t  response_;
        read_request request_;
    };
}