#pragma once
#include "common/macro_define.hpp"

namespace moon
{
    constexpr int32_t WORKER_ID_SHIFT = 24;
    constexpr int64_t UPDATE_INTERVAL = 10; //ms
    constexpr int32_t BUFFER_HEAD_RESERVED = 14;//max : websocket header  max  len

    DECLARE_UNIQUE_PTR(message);
    DECLARE_SHARED_PTR(buffer);
    DECLARE_UNIQUE_PTR(service);

    constexpr uint8_t PTYPE_UNKNOWN = 0;
    constexpr uint8_t PTYPE_SYSTEM = 1;
    constexpr uint8_t PTYPE_TEXT = 2;
    constexpr uint8_t PTYPE_LUA = 3;
    constexpr uint8_t PTYPE_SOCKET = 4;
    constexpr uint8_t PTYPE_ERROR = 5;
    constexpr uint8_t PTYPE_SOCKET_WS = 6; //websocket
    constexpr uint8_t PTYPE_DEBUG = 7;//

    //network
    using message_size_t = uint16_t;
    constexpr message_size_t MAX_NET_MSG_SIZE = 0x7FFF;

    constexpr  string_view_t STR_LF = "\n"sv;
    constexpr  string_view_t STR_CRLF = "\r\n"sv;
    constexpr  string_view_t STR_DCRLF = "\r\n\r\n"sv;

    enum class buffer_flag :uint8_t
    {
        pack_size = 1 << 0,
        close = 1 << 1,
        chunked = 1 << 2,
        broadcast = 1 << 3,
        ws_text = 1 << 4,
        ws_ping = 1 << 5,
        ws_pong = 1 << 6,
        buffer_flag_max,
    };

    enum class socket_data_type :std::uint8_t
    {
        socket_connect = 1,
        socket_accept = 2,
        socket_recv = 3,
        socket_close = 4,
        socket_error = 5,
        socket_ping = 6,
        socket_pong = 7,
    };

    enum class frame_enable_flag :std::uint8_t
    {
        none = 0,
        send = 1 << 0,//
        receive = 1 << 1,//
        both = 3,
    };

}





