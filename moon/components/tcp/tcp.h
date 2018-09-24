/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include "config.h"
#include "component.h"
#include "service.h"
#include "router.h"
#include "worker.h"

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
        socket_error = 5
    };

    enum class network_logic_error :std::uint8_t
    {
        ok = 0,
        read_message_size_max = 1, // read message size too long
        send_message_size_max = 2, // send message size too long
        timeout = 3, //socket read time out
    };

	inline const char* logic_errmsg(int logic_errcode)
	{
		static const char* errmsg[] = { 
			"ok",
			"read message size too long",
			"send message size too long",
			"timeout" 
		};
		return errmsg[logic_errcode];
	}

    enum class read_delim :std::uint8_t
    {
        FIXEDLEN,
        CRLF,//\r\n
        DCRLF,// \r\n\r\n
        LF,// \n
    };

    enum class frame_enable_flag :std::uint8_t
    {
        none = 0,
        send = 1<<0,//
        receive = 1<<1,//
        both = 3,
    };

	class base_connection;

    class tcp:public component
    {
		using connection_ptr_t = std::shared_ptr<base_connection>;
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

		asio::io_service& io_service();

		uint32_t make_connid();

		void make_response(string_view_t data, const std::string& header, int32_t responseid, uint8_t mtype = PTYPE_SOCKET);

		connection_ptr_t create_connection();
    private:
		asio::io_service* ios_;
		uint32_t connuid_;
		uint32_t timeout_;
		protocol_type type_;
		frame_enable_flag frame_type_;
		service* parent_;
		std::shared_ptr<asio::ip::tcp::acceptor> acceptor_;
		std::shared_ptr<asio::steady_timer> checker_;
		message_ptr_t  response_msg_;
		std::unordered_map<uint32_t, connection_ptr_t> conns_;
    };
}






