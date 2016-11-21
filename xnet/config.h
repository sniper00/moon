/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include "macro_define.h"

#define MTYPE_UNKNOWN	0
#define MTYPE_SERVICE			1
#define MTYPE_RESPONSE		2
#define MTYPE_CLIENT			3
#define MTYPE_SYSTEM			4
#define MTYPE_BROADCAST	5
#define MTYPE_NCONNECT	6
#define MTYPE_NDATA			7
#define MTYPE_NCLOSE			8
#define MTYPE_CRASH			9

#define IO_BUFFER_SIZE	4096
#define MAX_NMSG_SIZE 8192

namespace moon
{
	struct message_head
	{
		uint16_t len;
	};

	DECLARE_SHARED_PTR(message);
	DECLARE_SHARED_PTR(buffer);

	using network_handle_t = std::function<void(const message_ptr_t&)>;
}




