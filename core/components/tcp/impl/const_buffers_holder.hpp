/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
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
            if (buf->check_flag(uint8_t(buffer_flag::close)))
            {
                close_ = true;
            }
            buffers_.push_back(asio::buffer(buf->data(), buf->size()));
            datas_.push_back(buf);
        }

        void push_back(buffer_ptr_t&& buf)
        {
            if (buf->check_flag(uint8_t(buffer_flag::close)))
            {
                close_ = true;
            }
            buffers_.push_back(asio::buffer(buf->data(), buf->size()));
            datas_.push_back(std::forward<buffer_ptr_t>(buf));
        }

        const auto& buffers() const
        {
            return buffers_;
        }

        size_t size() const
        {
            return datas_.size();
        }

        void clear()
        {
            close_ = false;
            buffers_.clear();
            datas_.clear();
        }

        bool close() const
        {
            return close_;
        }
    private:
        bool close_ = false;
        std::vector<asio::const_buffer> buffers_;
        std::vector<buffer_ptr_t> datas_;
    };
}






