/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include "macro_define.h"

namespace moon
{
	DECLARE_SHARED_PTR(message);
	DECLARE_SHARED_PTR(buffer);
	DECLARE_SHARED_PTR(service);

	enum class message_type
	{
		unknown = 0,
		message_system = 0x01,

		service_send = 0x11,
		service_response = 0x12,
		service_client = 0x13,

		network_connect = 0x21,
		network_recv = 0x22,
		network_close = 0x23,
		network_error = 0x24,
		network_logic_error = 0x25
	};

	inline bool is_service_message(message_type v)
	{
		return (uint8_t(v) >> 4 == 0x1);
	}

	inline bool is_network_message(message_type v)
	{
		return (uint8_t(v) >> 4 == 0x2);
	}

	using message_size_t = uint16_t;

	enum class buffer_flag :uint8_t
	{
		pack_size = 1
	};
}




