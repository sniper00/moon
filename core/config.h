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
    const int32_t WORKER_ID_SHIFT = 24;
    const int64_t EVENT_UPDATE_INTERVAL = 5;
    const size_t BUFFER_HEAD_RESERVED = 6;
    const size_t SERVICE_DB_NUM = 5;

    DECLARE_SHARED_PTR(message);
    DECLARE_SHARED_PTR(buffer);
    DECLARE_SHARED_PTR(service);

    const uint8_t PTYPE_UNKNOWN = 0;
    const uint8_t PTYPE_SYSTEM = 1;
    const uint8_t PTYPE_TEXT = 2;
    const uint8_t PTYPE_LUA = 3;
    const uint8_t PTYPE_SOCKET = 4;
    const uint8_t PTYPE_ERROR = 5;

    //network
    using message_size_t = uint16_t;
    const message_size_t MAX_NMSG_SIZE = 8192;

    inline uint8_t worker_id(uint32_t serviceid)
    {
        return ((serviceid >> WORKER_ID_SHIFT) & 0xFF);
    }
}




