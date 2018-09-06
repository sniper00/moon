/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include "common/macro_define.hpp"

namespace moon
{
	constexpr int32_t WORKER_ID_SHIFT = 24;
	constexpr int64_t EVENT_UPDATE_INTERVAL = 10;
	constexpr size_t BUFFER_HEAD_RESERVED = 8;
	constexpr size_t MAX_REQUEST_STREAMBUF_SIZE = 8192;

    DECLARE_SHARED_PTR(message);
    DECLARE_SHARED_PTR(buffer);
    DECLARE_SHARED_PTR(service);

	constexpr uint8_t PTYPE_UNKNOWN = 0;
	constexpr uint8_t PTYPE_SYSTEM = 1;
	constexpr uint8_t PTYPE_TEXT = 2;
	constexpr uint8_t PTYPE_LUA = 3;
	constexpr uint8_t PTYPE_SOCKET = 4;
	constexpr uint8_t PTYPE_ERROR = 5;

    //network
    using message_size_t = uint16_t;
    constexpr message_size_t MAX_NET_MSG_SIZE = 0x7FFF;

    inline uint8_t worker_id(uint32_t serviceid) noexcept
    {
        return ((serviceid >> WORKER_ID_SHIFT) & 0xFF);
    }

	constexpr  string_view_t STR_LF = "\n"sv;
	constexpr  string_view_t STR_CRLF = "\r\n"sv;
	constexpr  string_view_t STR_DCRLF = "\r\n\r\n"sv;

	enum class buffer_flag:uint8_t
	{
		pack_size = 1 << 0,
		close = 1 << 1,
		framing = 1 << 2,
		broadcast = 1 << 3,
	};
}




