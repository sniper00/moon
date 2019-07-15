#pragma once
#include "config.hpp"
#include "common/rwlock.hpp"
#include "common/utils.hpp"
#include "asio.hpp"
#include "service.hpp"

namespace moon
{
    enum class socket_data_type :std::uint8_t
    {
        socket_connect = 1,
        socket_accept = 2,
        socket_recv = 3,
        socket_close = 4,
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
        send = 1 << 0,//
        receive = 1 << 1,//
        both = 3,
    };

    class router;
    class worker;
    class service;
    class base_connection;

    using connection_ptr_t = std::shared_ptr<base_connection>;

    class socket
    {
        struct acceptor_context
        {
            acceptor_context(uint8_t t, uint32_t o, asio::io_context& ioc)
                :type(t)
                , owner(o)
                , acceptor(ioc)
            {

            }

            uint8_t type;
            uint32_t owner;
            uint32_t fd = 0;
            asio::ip::tcp::acceptor acceptor;
        };

        using acceptor_context_ptr_t = std::shared_ptr<acceptor_context>;
    public:
        friend class base_connection;

        static constexpr size_t max_socket_num = 0xFFFF;

        socket(router* r, worker* w, asio::io_context& ioctx);

        socket(const socket&) = delete;

        socket& operator =(const socket&) = delete;

        uint32_t listen(const std::string& host, uint16_t port, uint32_t owner, uint8_t type);

        void accept(int fd, int32_t sessionid, uint32_t owner);

        int connect(const std::string& host, uint16_t port, uint32_t serviceid, uint32_t owner, uint8_t type, int32_t sessionid, int32_t timeout = 0);

        void read(uint32_t fd, uint32_t owner, size_t n, read_delim delim, int32_t sessionid);

        bool write(uint32_t fd, const buffer_ptr_t & data);

        bool write_with_flag(uint32_t fd, const buffer_ptr_t & data, int flag);

        bool write_message(uint32_t fd, message * msg);

        bool close(uint32_t fd, bool remove = false);

        bool settimeout(uint32_t fd, int v);

        bool setnodelay(uint32_t fd);

        bool set_enable_frame(uint32_t fd, std::string flag);
    private:
        uint32_t uuid();

        connection_ptr_t make_connection(uint32_t serviceid, uint8_t type);

        void response(uint32_t sender, uint32_t receiver, string_view_t data, string_view_t header, int32_t sessionid, uint8_t type);

        bool try_lock_fd(uint32_t fd);

        void unlock_fd(uint32_t fd);

        void add_connection(const connection_ptr_t& c, bool accepted);

        template<typename Message>
        void handle_message(uint32_t serviceid, Message&& m);

        service* find_service(uint32_t serviceid);

        void timeout();
    private:
        std::atomic<uint32_t> uuid_ = 0;
        router* router_;
        worker* worker_;
        asio::io_context& ioc_;
        asio::steady_timer timer_;
        message_ptr_t  response_;
        mutable rwlock lock_;
        std::unordered_map<uint32_t, acceptor_context_ptr_t> acceptors_;
        std::unordered_map<uint32_t, connection_ptr_t> connections_;
        std::unordered_set<uint32_t> fd_watcher_;
    };

    template<typename Message>
    void socket::handle_message(uint32_t serviceid, Message&& m)
    {
        auto s = find_service(serviceid);
        if (nullptr == s)
        {
            close(m->sender());
            return;
        }

        auto type = m->type();
        auto subtype = m->subtype();
        auto sender = m->sender();

        m->set_receiver(0);

        s->handle_message(std::forward<Message>(m));
        if ((0 != sender) && (type == PTYPE_ERROR || subtype == static_cast<uint8_t>(socket_data_type::socket_close)))
        {
            MOON_CHECK(close(sender,true), "close failed");
        }
    }
}
