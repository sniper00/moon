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
	buffer_ptr_t MOON_EXPORT create_buffer(size_t capacity = 64, size_t headreserved = sizeof(message_size_t));

	class MOON_EXPORT  message final
	{
	public:
		static message_ptr_t create(size_t capacity = 64, size_t headreserved = sizeof(message_size_t));

		message(size_t capacity = 64, size_t headreserved = 0);

		message(const buffer_ptr_t & v);

		~message();

		message(const message&) = delete;

		message& operator=(const message&) = delete;

		void set_sender(uint32_t serviceid);

		uint32_t sender() const;

		void set_receiver(uint32_t serviceid);

		uint32_t receiver() const;

		void set_userctx(const std::string& v);

		const std::string userctx();

		void set_rpc(uint32_t v);

		uint32_t rpc() const;

		void set_type(message_type v);

		message_type type() const;

		std::string bytes() const;

		void write_data(const std::string& v);

		const uint8_t* data() const;

		size_t size() const;

		operator buffer&();

		const buffer_ptr_t to_buffer() const;

		bool broadcast();

		void set_broadcast(bool v);

		void reset();

		uint16_t check_uint16();

	private:
		struct message_imp;
		message_imp* imp_;
	};
};


