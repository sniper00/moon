/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include "config.h"
#include "common/buffer.hpp"

namespace moon
{
    class  message final
    {
    public:
        static buffer_ptr_t create_buffer(size_t capacity = 64, size_t headreserved = BUFFER_HEAD_RESERVED)
        {
            return std::make_shared<buffer>(capacity, headreserved);
        }

        static message_ptr_t create(size_t capacity = 64, size_t headreserved = BUFFER_HEAD_RESERVED)
        {
            return std::make_shared<message>(capacity, headreserved);
        }

        static message_ptr_t create(const buffer_ptr_t & v)
        {
            return std::make_shared<message>(v);
        }

        message(size_t capacity = 64, size_t headreserved = 0)
        {
            init();
            data_ = std::make_shared<buffer>(capacity, headreserved);
        }

        explicit message(const buffer_ptr_t & v)
        {
            init();
            data_ = v;
        }

        ~message()
        {
        }

        message(const message&) = delete;

        message& operator=(const message&) = delete;

        void set_sender(uint32_t serviceid)
        {
            sender_ = serviceid;
        }

        uint32_t sender() const
        {
            return sender_;
        }

        void set_receiver(uint32_t serviceid)
        {
            receiver_ = serviceid;
        }

        uint32_t receiver() const
        {
            return receiver_;
        }

        void set_header(string_view_t header)
        {
            if (header.size() != 0)
            {
				if (nullptr == header_)
				{
					header_ = std::make_unique<std::string>(header.data(), header.size());
				}
				else
				{
					header_->clear();
					header_->assign(header.data(), header.size());
				}
            }       
        }

		string_view_t header() const
        {
			if (nullptr == header_)
			{
				return string_view_t{};
			}
			else
			{
				return string_view_t{ header_->data(),header_->size() };
			}
        }

        void set_responseid(int32_t v)
        {
            responseid_ = v;
        }

        int32_t responseid() const
        {
            return responseid_;
        }

        void set_type(uint8_t v)
        {
            type_ = v;
        }

        uint8_t type() const
        {
            return type_;
        }

        void set_subtype(uint8_t v)
        {
            subtype_ = v;
        }

        uint8_t subtype() const
        {
            return subtype_;
        }

        string_view_t bytes() const
        {
            if (nullptr == data_)
            {
                return string_view_t(nullptr, 0);
            }
            return string_view_t(reinterpret_cast<const char*>(data_->data()),data_->size());
        }

        string_view_t substr(int pos, size_t len = string_view_t::npos) const
        {
            string_view_t sr(reinterpret_cast<const char*>(data_->data()), data_->size());
            return sr.substr(pos, len);
        }

        void write_data(string_view_t s)
        {
            data_->write_back(s.data(), 0, s.size());
        }

        void write_string(const std::string& s)
        {
            data_->write_back(s.data(), 0, s.size()+1);
        }

        const char* data() const
        {
            return data_->data();
        }

        size_t size() const
        {
            return data_->size();
        }

        operator const buffer_ptr_t&() const
        {
            return data_;
        }

        buffer* get_buffer()
        {
            return data_.get();
        }

        bool broadcast() const
        {
            return data_->has_flag(static_cast<uint8_t>(buffer_flag::broadcast));
        }

        void set_broadcast(bool v)
        {
            v?data_->set_flag(static_cast<uint8_t>(buffer_flag::broadcast)): data_->clear_flag(static_cast<uint8_t>(buffer_flag::broadcast));
        }

        void* pointer()
        {
            return data_.get();
        }

        void reset()
        {
           init();
		   if (nullptr != header_)
		   {
			   header_->clear();
		   }
           data_->clear();
        }
    private:
        void init()
        {
            type_ = 0;
            subtype_ = 0;
            sender_ = 0;
            receiver_ = 0;
            responseid_ = 0;
        }

    private:
        uint8_t type_;
        uint8_t subtype_;
        uint32_t sender_;
        uint32_t receiver_;
        int32_t responseid_;
        std::unique_ptr<std::string> header_;
        buffer_ptr_t data_;
    };
};


