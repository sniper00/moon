/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include "config.h"
#include "common/buffer.hpp"

namespace moon
{
	DECLARE_SHARED_PTR(buffer)
	DECLARE_SHARED_PTR(message)

	class message
	{
	public:
		using stream_type = buffer;

		message(size_t capacity = 64, size_t headreserved = 0)
		{
			init();
			data_ = std::make_shared<buffer>(capacity, headreserved);
		}

		message(const buffer_ptr_t & v)
		{
			init();
			data_ = v;
		}

		message(const message& msg) = default;

		message(message&& msg) = default;

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

		void set_userctx(const std::string& v)
		{
			userctx_ = std::move(v);
		}

		const std::string userctx()
		{
			return userctx_;
		}

		void set_rpc(uint32_t v)
		{
			rpc_ = v;
		}

		uint32_t rpc() const
		{
			return rpc_;
		}

		void set_type(uint8_t v)
		{
			type_  = v;
		}

		uint8_t type() const
		{
			return type_;
		}

		std::string bytes() const
		{
			return std::string((char*)data_->data(), data_->size());
		}
	
		void write_data(const std::string& v)
		{
			data_->write_back(v.data(), 0, v.size());
		}

		const uint8_t* data() const
		{
			return data_->data();
		}
	
		size_t size() const
		{
			return data_->size();
		}

		operator buffer&()
		{
			return *(data_.get());
		}

		buffer_ptr_t to_buffer()
		{
			return data_;
		}

	protected:
		void init()
		{
			type_ = MTYPE_UNKNOWN;
			sender_ = 0;
			receiver_ = 0;
			rpc_ = 0;
		}
	protected:
		uint8_t type_;
		uint32_t sender_;
		uint32_t receiver_;
		uint32_t rpc_;
		std::string userctx_;
		buffer_ptr_t data_;
	};
};


