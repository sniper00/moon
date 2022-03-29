#include "socket.h"
#include "common/string.hpp"
#include "common/hash.hpp"
#include "worker.h"
#include "server.h"

#include "network/moon_connection.hpp"
#include "network/stream_connection.hpp"
#include "network/ws_connection.hpp"

using namespace moon;

socket::socket(server* s, worker* w, asio::io_context & ioctx)
    : server_(s)
    , worker_(w)
    , ioc_(ioctx)
    , timer_(ioctx)
    , response_()
{
    timeout();
}

bool socket::try_open(const std::string& host, uint16_t port)
{
    try
    {
        asio::ip::tcp::resolver resolver(ioc_);
        asio::ip::tcp::endpoint endpoint = *resolver.resolve(host, std::to_string(port)).begin();
        asio::ip::tcp::acceptor acceptor{ ioc_ };
        acceptor.open(endpoint.protocol());
#if TARGET_PLATFORM != PLATFORM_WINDOWS
        acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
#endif
        acceptor.bind(endpoint);
        acceptor.close();
        return true;
    }
    catch (asio::system_error& e)
    {
        CONSOLE_ERROR(server_->logger(), "%s:%d %s(%d)", host.data(), port, e.what(), e.code().value());
        return false;
    }
}

uint32_t socket::listen(const std::string & host, uint16_t port, uint32_t owner, uint8_t type)
{
    try
    {
        auto ctx = std::make_shared<socket::acceptor_context>(type, owner, ioc_);
        asio::ip::tcp::resolver resolver(ioc_);
        asio::ip::tcp::endpoint endpoint = *resolver.resolve(host, std::to_string(port)).begin();
        ctx->acceptor.open(endpoint.protocol());
#if TARGET_PLATFORM != PLATFORM_WINDOWS
        ctx->acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
#endif
        ctx->acceptor.bind(endpoint);
        ctx->acceptor.listen(std::numeric_limits<int>::max());

        auto id = server_->nextfd();
        ctx->fd = id;
        acceptors_.emplace(id, ctx);
        return id;
    }
    catch (asio::system_error& e)
    {
        CONSOLE_ERROR(server_->logger(), "%s:%d %s(%d)", host.data(), port, e.what(), e.code().value());
        return 0;
    }
}

uint32_t socket::udp(uint32_t owner, std::string_view host, uint16_t port)
{
    try
    {
        udp_context_ptr_t ctx;
        if (host.empty())
        {
            ctx = std::make_shared<socket::udp_context>(owner, ioc_);
        }
        else
        {
            asio::ip::udp::resolver resolver(ioc_);
            asio::ip::udp::endpoint endpoint = *resolver.resolve(host, std::to_string(port)).begin();
            ctx = std::make_shared<socket::udp_context>(owner, ioc_, endpoint);
        }
        auto id = server_->nextfd();
        ctx->fd = id;
        do_receive(ctx);
        udp_.emplace(id, std::move(ctx));
        return id;
    }
    catch (asio::system_error& e)
    {
        CONSOLE_ERROR(server_->logger(), "%s:%d %s(%d)", host.data(), port, e.what(), e.code().value());
        return 0;
    }
}

bool socket::udp_connect(uint32_t fd, std::string_view host, uint16_t port)
{
    try
    {
        if (auto iter = udp_.find(fd); iter != udp_.end())
        {
            asio::ip::udp::resolver resolver(ioc_);
            asio::ip::udp::endpoint endpoint = *resolver.resolve(host, std::to_string(port)).begin();
            iter->second->sock.connect(endpoint);
            return true;
        }
        return false;
    }
    catch (asio::system_error& e)
    {
        CONSOLE_ERROR(server_->logger(), "%s:%d %s(%d)", host.data(), port, e.what(), e.code().value());
        return false;
    }
}

void socket::accept(uint32_t fd, int32_t sessionid, uint32_t owner)
{
    assert(owner > 0 && "socket::accept : invalid serviceid");
    auto iter = acceptors_.find(fd);
    if (iter == acceptors_.end())
    {
        return;
    }

    auto& ctx = iter->second;

    if (!ctx->acceptor.is_open())
    {
        return;
    }

    worker* w = server_->get_worker(0, owner);
    auto c = w->socket().make_connection(owner, ctx->type);

    ctx->acceptor.async_accept(c->socket(), [this, ctx, c, w, sessionid, owner](const asio::error_code& e)
    {
        if (!e)
        {
            c->fd(server_->nextfd());
            w->socket().add_connection(this, ctx, c, sessionid);
        }
        else
        {
            if (sessionid != 0)
            {
                response(ctx->fd, ctx->owner, moon::format("socket::accept error %s(%d)", e.message().data(), e.value()), "SOCKET_ERROR"sv, sessionid, PTYPE_ERROR);
            }
            else
            {
                if (e != asio::error::operation_aborted)
                {
                    CONSOLE_WARN(server_->logger(), "socket::accept error %s(%d)", e.message().data(), e.value());
                }
            }
        }

        if (sessionid == 0)
        {
            accept(ctx->fd, sessionid, owner);
        }
    });
}

uint32_t socket::connect(const std::string& host, uint16_t port, uint32_t owner, uint8_t type, int32_t sessionid, uint32_t millseconds)
{
    if (0 == sessionid)
    {
        try
        {
            asio::ip::tcp::resolver resolver(ioc_);
            asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));
            auto c = make_connection(owner, type);
            asio::connect(c->socket(), endpoints);
            c->fd(server_->nextfd());
            connections_.emplace(c->fd(), c);
            asio::post(ioc_, [c]() {c->start(false);});
            return c->fd();
        }
        catch (asio::system_error& e)
        {
            CONSOLE_WARN(server_->logger(), "connect %s:%d failed: %s(%d)", host.data(), port, e.code().message().data(), e.code().value());
        }
    }
    else
    {
        std::shared_ptr<asio::ip::tcp::resolver> resolver = std::make_shared<asio::ip::tcp::resolver>(ioc_);
        resolver->async_resolve(host, std::to_string(port),
            [this, millseconds, type, owner, sessionid, host, port, resolver](const asio::error_code& ec, asio::ip::tcp::resolver::results_type results)
            {
                if (!ec)
                {
                    auto c = make_connection(owner, type);
                    std::shared_ptr<asio::steady_timer> timer;
                    if (millseconds > 0)
                    {
                        timer = std::make_shared<asio::steady_timer>(ioc_);
                        timer->expires_after(std::chrono::milliseconds(millseconds));
                        timer->async_wait([this, c, owner, sessionid, host, port, timer](const asio::error_code& ec) {
                            if (!ec)
                            {
                                // The timer may have expired, but the callback function has not yet been called(asio's complete-queue).(0 == timer->cancel()).
                                // Only trigger error code when socket not connected :
                                if (c->fd() == 0)
                                {
                                    c->close();
                                    response(0, owner, std::string_view{}, moon::format("connect %s:%d: timeout", host.data(), port), sessionid, PTYPE_ERROR);
                                }
                            }
                            });
                    }

                    asio::async_connect(c->socket(), results,
                        [this, c, host, port, owner, sessionid, timer](const asio::error_code& ec, const asio::ip::tcp::endpoint&)
                        {
                            if (timer)
                            {
                                try {
                                    timer->cancel();
                                }
                                catch (...) {
                                }
                            }

                            if (!ec)
                            {
                                c->fd(server_->nextfd());
                                connections_.emplace(c->fd(), c);
                                c->start(false);
                                response(0, owner, std::to_string(c->fd()), std::string_view{}, sessionid, PTYPE_TEXT);
                            }
                            else
                            {
                                if(c->socket().is_open())
                                    response(0, owner, std::string_view{}, moon::format("connect %s:%d: %s(%d)", host.data(), port, ec.message().data(), ec.value()), sessionid, PTYPE_ERROR);
                            }
                        });
                }
                else
                {
                    response(0, owner, std::string_view{}, moon::format("resolve %s:%d: %s(%d)", host.data(), port, ec.message().data(), ec.value()), sessionid, PTYPE_ERROR);
                }
            });
    }
    return 0;
}

void socket::read(uint32_t fd, uint32_t owner, size_t n, std::string_view delim, int32_t sessionid)
{
    if (auto iter = connections_.find(fd); iter != connections_.end())
    {
        iter->second->read(n, delim, sessionid);
        return;
    }

    asio::post(ioc_, [this, owner, sessionid]() {
        response(0, owner, "read an invalid socket", "closed", sessionid, PTYPE_ERROR);
    });
}

bool socket::write(uint32_t fd, buffer_ptr_t data, buffer_flag flag)
{
    if (auto iter = connections_.find(fd); iter != connections_.end())
    {
        data->set_flag(flag);
        return iter->second->send(std::move(data));
    }

    if (auto iter = udp_.find(fd); iter != udp_.end())
    {
        iter->second->sock.async_send(
            asio::buffer(data->data(), data->size()),
            [data, ctx = iter->second](std::error_code /*ec*/, std::size_t /*bytes_sent*/)
            {
            });
        return true;
    }
    return false;
}

bool socket::close(uint32_t fd)
{
    if (auto iter = connections_.find(fd); iter != connections_.end())
    {
        iter->second->close();
        connections_.erase(iter);
        server_->unlock_fd(fd);
        return true;
    }

    if (auto iter = udp_.find(fd); iter != udp_.end())
    {
        iter->second->closed = true;
        iter->second->sock.close();
        udp_.erase(iter);
        server_->unlock_fd(fd);
        return true;
    }

    if (auto iter = acceptors_.find(fd); iter != acceptors_.end())
    {
        if (iter->second->acceptor.is_open())
        {
            iter->second->acceptor.cancel();
            iter->second->acceptor.close();
        }
        acceptors_.erase(iter);
        server_->unlock_fd(fd);
        return true;
    }
    return false;
}

void socket::close_all()
{
    for (auto& c : connections_)
    {
        c.second->close();
    }

    for (auto& c : udp_)
    {
        c.second->sock.close();
    }

    for (auto& ac : acceptors_)
    {
        if (ac.second->acceptor.is_open())
        {
            ac.second->acceptor.cancel();
            ac.second->acceptor.close();
        }
    }
}

bool socket::settimeout(uint32_t fd, uint32_t seconds)
{
    if (auto iter = connections_.find(fd); iter != connections_.end())
    {
        iter->second->settimeout(seconds);
        return true;
    }
    return false;
}

bool socket::setnodelay(uint32_t fd)
{
    if (auto iter = connections_.find(fd); iter != connections_.end())
    {
        iter->second->set_no_delay();
        return true;
    }
    return false;
}

bool socket::set_enable_chunked(uint32_t fd, std::string_view flag)
{
    int v = static_cast<int>(enable_chunked::none);
    for (const auto& c : flag)
    {
        switch (c)
        {
        case 'r':
        case 'R':
            v |= static_cast<int>(moon::enable_chunked::receive);
            break;
        case 'w':
        case 'W':
            v |= static_cast<int>(moon::enable_chunked::send);
            break;
        default:
            CONSOLE_WARN(server_->logger(),
                "tcp::set_enable_chunked Unsupported enable chunked flag %s.Support: 'r' 'w'.", flag.data());
            return false;
        }
    }

    if (auto iter = connections_.find(fd); iter != connections_.end())
    {
        auto c = std::dynamic_pointer_cast<moon_connection>(iter->second);
        if (c)
        {
            c->set_enable_chunked(static_cast<enable_chunked>(v));
            return true;
        }
    }
    return false;
}

bool socket::set_send_queue_limit(uint32_t fd, uint32_t warnsize, uint32_t errorsize)
{
    if (auto iter = connections_.find(fd); iter != connections_.end())
    {
        iter->second->set_send_queue_limit(warnsize, errorsize);
        return true;
    }
    return false;
}

static bool decode_endpoint(std::string_view address, asio::ip::udp::endpoint& ep)
{
    if (address[0] != '4' && address[0] != '6')
        return false;
    asio::ip::port_type port = 0;
    if (address[0] == '4')
    {
        address = address.substr(1);
        asio::ip::address_v4::bytes_type bytes;
        if ((address.size()) != bytes.size() + sizeof(port))
            return false;
        memcpy(bytes.data(), address.data(), bytes.size());
        memcpy(&port, address.data() + bytes.size(), sizeof(port));
        ep = asio::ip::udp::endpoint(asio::ip::address(asio::ip::make_address_v4(bytes)), port);
    }
    else
    {
        address = address.substr(1);
        asio::ip::address_v6::bytes_type bytes;
        if ((address.size()) != bytes.size() + sizeof(port))
            return false;
        memcpy(bytes.data(), address.data(), bytes.size());
        memcpy(&port, address.data() + bytes.size(), sizeof(port));
        ep = asio::ip::udp::endpoint(asio::ip::address(asio::ip::make_address_v6(bytes)), port);
    }
    return true;
}

bool socket::send_to(uint32_t host, std::string_view address, buffer_ptr_t data)
{
    if (address.empty())
        return false;
    if (auto iter = udp_.find(host); iter != udp_.end())
    {
        asio::ip::udp::endpoint ep;
        if (!decode_endpoint(address, ep))
            return false;
        iter->second->sock.async_send_to(
            asio::buffer(data->data(), data->size()), ep,
            [data, ctx = iter->second](std::error_code /*ec*/, std::size_t /*bytes_sent*/)
            {
            });
        return true;
    }
    return false;
}

std::string moon::socket::getaddress(uint32_t fd)
{
	if (auto iter = connections_.find(fd); iter != connections_.end())
	{
		return iter->second->address();
	}
	return std::string();
}

size_t socket::encode_endpoint(char* buf, const asio::ip::address& addr, asio::ip::port_type port)
{
    size_t size = 0;
    if (addr.is_v4())
    {
        buf[0] = '4';
        size+=1;
        auto bytes = addr.to_v4().to_bytes();
        memcpy(buf + size, bytes.data(), bytes.size());
        size += bytes.size();
    }
    else if (addr.is_v6())
    {
        buf[0] = '6';
        size += 1;
        auto bytes = addr.to_v6().to_bytes();
        memcpy(buf + size, bytes.data(), bytes.size());
        size += bytes.size();
    }
    memcpy(buf + size, &port, sizeof(port));
    size += sizeof(port);
    return size;
}

connection_ptr_t socket::make_connection(uint32_t serviceid, uint8_t type)
{
    connection_ptr_t connection;
    switch (type)
    {
    case PTYPE_SOCKET:
    {
        connection = std::make_shared<moon_connection>(serviceid, type, this, ioc_);
        break;
    }
    case PTYPE_TEXT:
    {
        connection = std::make_shared<stream_connection>(serviceid, type, this, ioc_);
        break;
    }
    case PTYPE_SOCKET_WS:
    {
        connection = std::make_shared<ws_connection>(serviceid, type, this, ioc_);
        break;
    }
    default:
        MOON_ASSERT(false, "Unknown socket protocol");
        break;
    }
    connection->logger(server_->logger());
    return connection;
}

void socket::response(uint32_t sender, uint32_t receiver, std::string_view data, std::string_view header, int32_t sessionid, uint8_t type)
{
    if (0 == sessionid)
        return;
    response_.set_sender(sender);
    response_.set_receiver(0);
    response_.get_buffer()->clear();
    response_.get_buffer()->write_back(data.data(), data.size());
    response_.set_header(header);
    response_.set_sessionid(sessionid);
    response_.set_type(type);

    handle_message(receiver, response_);
}

void socket::add_connection(socket* from, const acceptor_context_ptr_t& ctx, const connection_ptr_t & c, int32_t  sessionid)
{
    asio::dispatch(ioc_, [this, from, ctx, c, sessionid] {
        connections_.emplace(c->fd(), c);
        c->start(true);

        if (sessionid != 0)
        {
            asio::dispatch(from->ioc_, [from, ctx, sessionid, fd = c->fd()]{
                    from->response(ctx->fd, ctx->owner, std::to_string(fd), std::string_view{}, sessionid, PTYPE_TEXT);
                });
        }
    });
}

service * socket::find_service(uint32_t serviceid)
{
    return worker_->find_service(serviceid);;
}

void socket::timeout()
{
    timer_.expires_after(std::chrono::seconds(10));
    timer_.async_wait([this](const asio::error_code & e) {
        if (e)
        {
            return;
        }

        auto now = base_connection::now();
        for (auto& connection : connections_)
        {
            connection.second->timeout(now);
        }
        timeout();
    });
}

void socket::do_receive(const udp_context_ptr_t& ctx)
{
    if (ctx->closed)
        return;

    auto buf = ctx->msg.get_buffer();
    buf->clear();
    ctx->sock.async_receive_from(
        asio::buffer(buf->data() + buf->size(), buf->writeablesize()), ctx->from_ep,
        [this, ctx](std::error_code ec, std::size_t bytes_recvd)
        {
            if (!ec && bytes_recvd >0)
            {
                auto buf = ctx->msg.get_buffer();
                buf->commit(bytes_recvd);
                char arr[32];
                size_t size = encode_endpoint(arr, ctx->from_ep.address(), ctx->from_ep.port());
                buf->write_front(arr, size);
                ctx->msg.set_sender(ctx->fd);
                ctx->msg.set_receiver(0);
                ctx->msg.set_type(PTYPE_SOCKET_UDP);
                handle_message(ctx->owner, ctx->msg);
            }
            do_receive(ctx);
        });
}

