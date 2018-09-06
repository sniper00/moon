/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include "config.h"
#include "component.h"

namespace moon
{
    enum  class protocol_type:std::uint8_t
    {
        protocol_default,
        protocol_custom,
        protocol_websocket
    };

    enum class socket_data_type :std::uint8_t
    {
        socket_connect = 1,
        socket_accept =2,
        socket_recv = 3,
        socket_close =4,
        socket_error = 5,
        socket_logic_error = 6
    };

    enum class network_logic_error :std::uint8_t
    {
        ok = 0,
        read_message_size_max = 1, // recv message size too long
        send_message_size_max = 2, // send message size too long
        timeout = 3, //socket read time out
        client_close = 4, //closed
    };

    enum class read_delim :std::uint8_t
    {
        FIXEDLEN,
        CRLF,//\r\n
        DCRLF,// \r\n\r\n
        LF,// \n
    };

    enum class frame_enable_type :std::uint8_t
    {
        none = 0,
        send = 1<<0,//
        receive = 1<<1,//
        both = 3,
    };

    class tcp:public component
    {
    public:

        friend class base_connection;

        tcp() noexcept;

        virtual ~tcp();

        void setprotocol(std::string flag);

        void settimeout(int seconds);

        void setnodelay(uint32_t connid);

        void set_enable_frame(std::string flag);

        bool listen(const std::string& ip, const std::string& port);

        void async_accept(int32_t responseid);

        void async_connect(const std::string & ip, const std::string & port,int32_t responseid);

        uint32_t connect(const std::string & ip, const std::string & port);

        void read(uint32_t connid,size_t n, read_delim delim,int32_t responseid);

        bool send(uint32_t connid, const buffer_ptr_t& data);

        bool send_then_close(uint32_t connid, const buffer_ptr_t& data);

        bool send_message(uint32_t connid, message* msg);

        bool close(uint32_t connid);

    private:
        void init() override;

        void destroy() override;

        void handle_message(const message_ptr_t& msg);

        void check();
    private:
        struct imp;
        imp* imp_;
    };
}






