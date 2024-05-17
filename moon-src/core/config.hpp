#pragma once
#include "common/common.hpp"
#include "common/buffer.hpp"

namespace moon
{
    constexpr uint32_t WORKER_ID_SHIFT = 24;// support 255 worker threads.
    constexpr uint32_t WORKER_MAX_SERVICE = (1<<24)-1;// max service count per worker thread.

    constexpr uint8_t PTYPE_UNKNOWN = 0;
    constexpr uint8_t PTYPE_SYSTEM = 1;
    constexpr uint8_t PTYPE_TEXT = 2;
    constexpr uint8_t PTYPE_LUA = 3;
    constexpr uint8_t PTYPE_ERROR = 4;
    constexpr uint8_t PTYPE_DEBUG = 5;//
    constexpr uint8_t PTYPE_SHUTDOWN = 6;//
    constexpr uint8_t PTYPE_TIMER = 7;//
    constexpr uint8_t PTYPE_SOCKET_TCP = 8; //
    constexpr uint8_t PTYPE_SOCKET_UDP = 9;//
    constexpr uint8_t PTYPE_SOCKET_WS = 10;   // websocket
    constexpr uint8_t PTYPE_SOCKET_MOON = 11; //
    constexpr uint8_t PTYPE_INTEGER = 12; //

    constexpr  std::string_view STR_LF = "\n"sv;
    constexpr  std::string_view STR_CRLF = "\r\n"sv;
    constexpr  std::string_view STR_DCRLF = "\r\n\r\n"sv;

    enum class state
    {
        unknown,
        init,
        ready,
        stopping,
        stopped
    };

    enum class socket_send_mask :uint8_t
    {
        none = 0,
        close = 1 << 1,
        ws_text = 1 << 2,
        ws_ping = 1 << 3,
        ws_pong = 1 << 4,
        max_mask
    };

    template<>
    struct enum_enable_bitmask_operators<socket_send_mask>
    {
        static constexpr bool enable = true;
    };

    enum class socket_data_type :std::uint8_t
    {
        socket_connect = 1,
        socket_accept = 2,
        socket_recv = 3,
        socket_close = 4,
        socket_ping = 5,
        socket_pong = 6,
    };

    enum class enable_chunked :std::uint8_t
    {
        none = 0,
        send = 1 << 0,//
        receive = 1 << 1,//
        both = 3,
    };

    template<>
    struct enum_enable_bitmask_operators<enable_chunked>
    {
        static constexpr bool enable = true;
    };

    struct service_conf
    {
        bool unique = false;
        uint32_t threadid = 0;
        uint32_t creator = 0;
        int64_t session = 0;
        ssize_t memlimit = 0;
        std::string type;
        std::string name;
        std::string source;
        std::string params;
    };

    constexpr uint32_t BOOTSTRAP_ADDR = 0x01000001;//The first service's id

    using message_size_t = uint16_t;//PTYPE_SOCKET_MOON message length type
    using buffer_ptr_t = std::unique_ptr<buffer>;
    using buffer_shr_ptr_t = std::shared_ptr<buffer>;

    struct direct_read_result
    {
        direct_read_result(bool suc, std::string_view s)
            :success(suc), size(s.size()), data(s.data()){}

        size_t success : 1;
        size_t size : 63;
        const char* data = nullptr;
    };
}





