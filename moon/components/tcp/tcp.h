#pragma once
#include "config.h"
#include "common/utils.hpp"
#include "component.hpp"
#include "asio.hpp"
#include "service.h"

namespace moon
{
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
        send_message_queue_size_max = 4, // send message queue size too long
    };

	inline const char* logic_errmsg(int logic_errcode)
	{
		static const char* errmsg[] = { 
			"ok",
			"read message size too long",
			"send message size too long",
			"timeout",
            "send message queue size too long"
		};
        if (logic_errcode >= static_cast<int>(array_szie(errmsg)))
        {
            return "closed";
        }
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

        void setprotocol(uint8_t v);

        void settimeout(int seconds);

        void setnodelay(uint32_t connid);

        void set_enable_frame(std::string flag);

        bool listen(const std::string& ip, const std::string& port);

        void async_accept(int32_t responseid);

        void async_connect(const std::string & ip, const std::string & port,int32_t responseid);

        uint32_t connect(const std::string & ip, const std::string & port);

        void read(uint32_t connid,size_t n, read_delim delim,int32_t responseid);

        bool send(uint32_t connid, const buffer_ptr_t& data);

        bool send_with_flag(uint32_t connid, const buffer_ptr_t& data, int flag);

        bool send_message(uint32_t connid, message* msg);

        bool close(uint32_t connid);
    private:
        bool init(std::string_view) override;

        void destroy() override;

        template<typename TMsg>
        void handle_message(TMsg&& msg);

        void check();

		asio::io_context& io_context();

		uint32_t make_connid();

		void response(string_view_t data, string_view_t header, int32_t responseid, uint8_t mtype);

		connection_ptr_t create_connection();
    private:
        uint8_t type_;
        frame_enable_flag frame_flag_;
		uint32_t connuid_;
		uint32_t timeout_;
        asio::io_context* io_ctx_;
		service* parent_;
		std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
		std::unique_ptr<asio::steady_timer> checker_;
		message_ptr_t  response_msg_;
		std::unordered_map<uint32_t, connection_ptr_t> connections_;
    };

    template<typename TMsg>
    void tcp::handle_message(TMsg&& msg)
    {
        if (!ok())
        {
            return;
        }

        auto t = msg->type();
        auto st = msg->subtype();
        auto sender = msg->sender();

        if (t == 0)
        {
            msg->set_type(type_);
        }

        msg->set_receiver(0);

        parent_->handle_message(std::forward<TMsg>(msg));
        if (t == PTYPE_ERROR || st == static_cast<uint8_t>(socket_data_type::socket_close))
        {
            connections_.erase(sender);
        }
    }
}






