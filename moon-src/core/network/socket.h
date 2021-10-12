#pragma once
#include "config.hpp"
#include "common/rwlock.hpp"
#include "common/utils.hpp"
#include "asio.hpp"
#include "service.hpp"
#include "message.hpp"

namespace moon
{
    class worker;
    class service;
    class base_connection;

    using connection_ptr_t = std::shared_ptr<base_connection>;

    class socket
    {
    public:
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

        static constexpr size_t addr_v4_size = 1 + sizeof(asio::ip::address_v4::bytes_type) + sizeof(asio::ip::port_type);
        static constexpr size_t addr_v6_size = 1 + sizeof(asio::ip::address_v6::bytes_type) + sizeof(asio::ip::port_type);

        struct udp_context
        {
            udp_context(uint32_t o, asio::io_context& ioc, asio::ip::udp::endpoint ep)
                :owner(o)
                , sock(ioc, ep)
            {
                msg = message::create(size_t{ 1024 } - addr_v6_size, static_cast<uint32_t>(addr_v6_size));
            }

            udp_context(uint32_t o, asio::io_context& ioc)
                :owner(o)
                , sock(ioc, asio::ip::udp::endpoint(asio::ip::udp::v4(), 0))
            {
                msg = message::create(size_t{ 1024 } - addr_v6_size, static_cast<uint32_t>(addr_v6_size));
            }

            bool closed = false;
            uint32_t owner;
            uint32_t fd = 0;
            message_ptr_t msg;
            asio::ip::udp::socket sock;
            asio::ip::udp::endpoint from_ep;
        };

        using acceptor_context_ptr_t = std::shared_ptr<acceptor_context>;
        using udp_context_ptr_t = std::shared_ptr<udp_context>;
    public:
        friend class base_connection;

        socket(server* s, worker* w, asio::io_context& ioctx);

        socket(const socket&) = delete;

        socket& operator =(const socket&) = delete;

        bool try_open(const std::string& host, uint16_t port);

        uint32_t listen(const std::string& host, uint16_t port, uint32_t owner, uint8_t type);

        uint32_t udp(uint32_t owner, std::string_view host, uint16_t port);

        bool udp_connect(uint32_t fd, std::string_view host, uint16_t port);

        void accept(uint32_t fd, int32_t sessionid, uint32_t owner);

        uint32_t connect(const std::string& host, uint16_t port, uint32_t owner, uint8_t type, int32_t sessionid, uint32_t millseconds = 0);

        void read(uint32_t fd, uint32_t owner, size_t n, std::string_view delim, int32_t sessionid);

        bool write(uint32_t fd, buffer_ptr_t data, buffer_flag flag = buffer_flag::none);

        bool close(uint32_t fd);

        void close_all();

        bool settimeout(uint32_t fd, uint32_t seconds);

        bool setnodelay(uint32_t fd);

        bool set_enable_chunked(uint32_t fd, std::string_view flag);

        bool set_send_queue_limit(uint32_t fd, uint32_t warnsize, uint32_t errorsize);

        bool send_to(uint32_t host, std::string_view address, buffer_ptr_t data);

        std::string getaddress(uint32_t fd);

        static size_t encode_endpoint(char* buf, const asio::ip::address& addr, asio::ip::port_type port);
    private:
        connection_ptr_t make_connection(uint32_t serviceid, uint8_t type);

        void response(uint32_t sender, uint32_t receiver, std::string_view data, std::string_view header, int32_t sessionid, uint8_t type);

        void add_connection(socket* from, const acceptor_context_ptr_t& ctx, const connection_ptr_t& c, int32_t  sessionid);

        template<typename Message>
        void handle_message(uint32_t serviceid, Message&& m);

        service* find_service(uint32_t serviceid);

        void timeout();

        void do_receive(const udp_context_ptr_t& ctx);
    private:
        server* server_;
        worker* worker_;
        asio::io_context& ioc_;
        asio::steady_timer timer_;
        message_ptr_t  response_;
        std::unordered_map<uint32_t, acceptor_context_ptr_t> acceptors_;
        std::unordered_map<uint32_t, connection_ptr_t> connections_;
        std::unordered_map<uint32_t, udp_context_ptr_t> udp_;
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
        moon::handle_message(s, std::forward<Message>(m));
    }
}
