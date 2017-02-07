/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#include "message.h"

namespace moon
{
	struct message::message_imp
	{
		message_imp()
		{
			init();
		}

		void init()
		{
			type_ = uint8_t(message_type::unknown);
			sender_ = 0;
			receiver_ = 0;
			rpc_ = 0;
			broadcast_ = false;
		}

		uint8_t type_;
		bool broadcast_;
		uint32_t sender_;
		uint32_t receiver_;
		uint32_t rpc_;
		std::string userctx_;
		buffer_ptr_t data_;
	};

	
	message_ptr_t message::create(size_t capacity, size_t headreserved)
	{
		return std::make_shared<message>(capacity, headreserved);
	}

	message::message(size_t capacity, size_t headreserved)
		:imp_(new message_imp)
	{
		imp_->data_ = std::make_shared<buffer>(capacity, headreserved);
	}

	message::message(const buffer_ptr_t & v)
		:imp_(new message_imp)
	{
		imp_->data_ = v;
	}

	message::~message()
	{
		SAFE_DELETE(imp_);
	}

	void message::set_sender(uint32_t serviceid)
	{
		imp_->sender_ = serviceid;
	}

	uint32_t message::sender() const
	{
		return imp_->sender_;
	}

	void message::set_receiver(uint32_t serviceid)
	{
		imp_->receiver_ = serviceid;
	}

	uint32_t message::receiver() const
	{
		return imp_->receiver_;
	}

	void message::set_userctx(const std::string & v)
	{
		imp_->userctx_ = std::move(v);
	}

	const std::string message::userctx()
	{
		return imp_->userctx_;
	}

	void message::set_rpc(uint32_t v)
	{
		imp_->rpc_ = v;
	}

	uint32_t message::rpc() const
	{
		return imp_->rpc_;
	}

	void message::set_type(message_type v)
	{
		imp_->type_ = (uint8_t)v;
	}

	message_type message::type() const
	{
		return (message_type)imp_->type_;
	}

	std::string message::bytes() const
	{
		return std::string((char*)imp_->data_->data(), imp_->data_->size());
	}

	void message::write_data(const std::string & v)
	{
		imp_->data_->write_back(v.data(), 0, v.size());
	}

	const uint8_t * message::data() const
	{
		return imp_->data_->data();
	}

	size_t message::size() const
	{
		return imp_->data_->size();
	}

	message::operator buffer&()
	{
		return *(imp_->data_.get());
	}

	const buffer_ptr_t message::to_buffer() const
	{
		return imp_->data_;
	}

	bool message::broadcast()
	{
		return imp_->broadcast_;
	}

	void message::set_broadcast(bool v)
	{
		imp_->broadcast_ = v;
	}

	void message::reset()
	{
		imp_->init();
		imp_->userctx_.clear();
		imp_->data_->clear();
	}

	uint16_t message::check_uint16()
	{
		if (imp_->data_->size() >= sizeof(uint16_t))
		{
			auto d = imp_->data_->data();
			if (imp_->data_->check_flag(uint8_t(buffer_flag::pack_size)))
			{
				if (imp_->data_->size() < 2 * sizeof(uint16_t))
				{
					return 0;
				}
				d += 2;
			}		
			return (*(uint16_t*)d);
		}
		return 0;
	}

	buffer_ptr_t create_buffer(size_t capacity, size_t headreserved)
	{
		return std::make_shared<buffer>(capacity, headreserved);
	}
};


