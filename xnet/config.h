/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include "macro_define.h"

#define IO_BUFFER_SIZE	4096
#define MAX_NMSG_SIZE 8192

namespace moon
{
	enum class message_type
	{
		unknown = 0,

		service_begin = 1,
		service_send,
		service_response,
		service_client,
		service_end ,

		network_begin = 100,
		network_connect,
		network_recv ,
		network_close,
		network_error,
		network_logic_error,
		network_end,

		system_begin = 200,
		system_service_crash,
		system_network_report,
		system_end
	};

	enum class network_logic_error
	{
		ok = 0,
		remote_endpoint = 1,// parse remote endpoint error
		message_size_max = 2, // send message size to long
		socket_shutdown = 3,//socket shutdown failed
		socket_close = 4,// socket close failed
		socket_setoption = 5,//socket set option failed
		socket_read = 6, //socket read failed
		socket_read_timeout = 7, //socket read failed
		socket_send = 8, //socket send failed
	};

	struct message_head
	{
		uint16_t len;
	};

	DECLARE_SHARED_PTR(message);
	DECLARE_SHARED_PTR(buffer);
	DECLARE_SHARED_PTR(service);

	using network_handle_t = std::function<void(const message_ptr_t&)>;

	inline bool is_service_message(message_type v)
	{
		return (v > (message_type::service_begin) && v < (message_type::service_end));
	}
}




