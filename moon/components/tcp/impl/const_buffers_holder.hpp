#pragma once
#include "config.h"
#include "common/buffer.hpp"
#include "asio.hpp"

namespace moon
{
    class const_buffers_holder
    {
    public:
        const_buffers_holder() = default;

        void push_back(const buffer_ptr_t& buf)
        {
            if (buf->has_flag(buffer_flag::close))
            {
                close_ = true;
            }
            buffers_.push_back(asio::buffer(buf->data(), buf->size()));
            datas_.push_back(buf);
        }

        void push_back(buffer_ptr_t&& buf)
        {
            if (buf->has_flag(buffer_flag::close))
            {
                close_ = true;
            }
            buffers_.push_back(asio::const_buffer(buf->data(), buf->size()));
            datas_.push_back(std::forward<buffer_ptr_t>(buf));
        }

        void framing_begin(const buffer_ptr_t& buf, size_t framing_size)
        {
            datas_.push_back(buf);
            headers_.reserve(framing_size);
        }

        void push_framing(message_size_t header, const char* data, size_t len)
        {
            headers_.push_back(header);
            message_size_t& back = headers_.back();
            buffers_.push_back(asio::buffer((const char*)&back, sizeof(back)));
            buffers_.push_back(asio::buffer(data, len));
        }

        const auto& buffers() const
        {
            return buffers_;
        }

        size_t size() const
        {
            return buffers_.size();
        }

        void clear()
        {
            close_ = false;
            buffers_.clear();
            datas_.clear();
            headers_.clear();
        }

        bool close() const
        {
            return close_;
        }
    private:
        bool close_ = false;
        std::vector<asio::const_buffer> buffers_;
        std::vector<buffer_ptr_t> datas_;
        std::vector<message_size_t> headers_;
    };
}






