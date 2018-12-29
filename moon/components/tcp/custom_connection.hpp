#pragma once
#include "base_connection.hpp"

namespace moon
{
    class custom_connection : public base_connection
    {
    public:
        using base_connection_t = base_connection;

        template <typename... Args>
        explicit custom_connection(Args&&... args)
            :base_connection_t(std::forward<Args>(args)...)
            , restore_write_offset_(0)
            , prew_read_offset_(0)
            , buffer_()
        {
        }

        void start(bool accepted, int32_t responseid = 0) override
        {
            base_connection_t::start(accepted, responseid);
            response_msg_ = message::create(8192);
            read_some();
        }

        bool read(const read_request& ctx) override
        {
            if (is_open() && read_request_.responseid == 0)
            {
                read_request_ = ctx;
                restore_buffer_offset();
                if (response_msg_->size() > 0)
                {
                    //guarantee read is async operation
                    asio::post(socket_.get_io_context(),[this, self = shared_from_this()] {
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
            socket_.async_read_some(asio::buffer(buffer_),
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
                    read_some();
                    return;
                }

                //CONSOLE_DEBUG(logger(), "connection recv:%u %s",id_, moon::to_hex_string(string_view_t{ (char*)buffer_.data(), bytes_transferred }, " ").data(), "");

                last_recv_time_ = now() / 1000;
                restore_buffer_offset();
                response_msg_->get_buffer()->write_back(buffer_.data(), 0, bytes_transferred);
                handle_read_request();
                read_some();
            }));
        }

        void restore_buffer_offset()
        {
            auto buf = response_msg_->get_buffer();
            buf->offset_writepos(restore_write_offset_);
            buf->seek(prew_read_offset_, buffer::Current);
            restore_write_offset_ = 0;
            prew_read_offset_ = 0;
        }

        void read_with_delim(buffer* buf, const string_view_t& delim)
        {
            size_t dszie = buf->size();
            if (read_request_.size != 0 && dszie > read_request_.size)
            {
                error(asio::error_code(), int(network_logic_error::read_message_size_max));
                base_connection_t::close();
                return;
            }

            string_view_t strref(buf->data(), dszie);
            size_t pos = strref.find(delim);
            if (pos != string_view_t::npos)
            {
                buf->offset_writepos(-static_cast<int>(dszie - pos));
                restore_write_offset_ = static_cast<int>(dszie - pos);
                prew_read_offset_ = static_cast<int>(pos + delim.size());
                response(response_msg_);
            }
        }

        void handle_read_request()
        {
            auto buf = response_msg_->get_buffer();
            size_t dszie = buf->size();

            if (0 == read_request_.responseid || dszie == 0)
            {
                return;
            }

            switch (read_request_.delim)
            {
            case read_delim::FIXEDLEN:
            {
                if (buf->size() >= read_request_.size)
                {
                    buf->offset_writepos(-static_cast<int>(dszie - read_request_.size));
                    restore_write_offset_ = static_cast<int>(dszie - read_request_.size);
                    prew_read_offset_ = static_cast<int>(read_request_.size);
                    response(response_msg_);
                    break;
                }
                else
                {
                    return;
                }
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

        void error(const asio::error_code& e, int logicerr, const char* lerrmsg = nullptr) override
        {
            (void)lerrmsg;

            response_msg_->get_buffer()->clear();

            if (e && e != asio::error::eof)
            {
                response_msg_->set_header("closed");
                response_msg_->write_string(moon::format("%s.(%d)", e.message().data(), e.value()));
            }
            else
            {
                response_msg_->set_header(logic_errmsg(logicerr));
            }

            if (read_request_.responseid != 0)
            {
                response(response_msg_, PTYPE_ERROR);
            }
        }

        void response(const message_ptr_t & msg, uint8_t mtype = PTYPE_TEXT)
        {
            msg->set_type(mtype);
            msg->set_responseid(read_request_.responseid);
            read_request_.responseid = 0;
            handle_message(msg);
        }
    protected:
        int restore_write_offset_;
        int prew_read_offset_;
        message_ptr_t  response_msg_;
        std::array<uint8_t, 8192> buffer_;
        read_request read_request_;
    };
}