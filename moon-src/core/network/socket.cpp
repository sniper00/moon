#include "socket.h"
#include "common/log.hpp"
#include "common/string.hpp"
#include "common/hash.hpp"
#include "worker.h"

#include "network/moon_connection.hpp"
#include "network/stream_connection.hpp"
#include "network/ws_connection.hpp"

using namespace moon;

socket::socket(router * r, worker* w, asio::io_context & ioctx)
    : router_(r)
    , worker_(w)
    , ioc_(ioctx)
    , timer_(ioctx)
{
    response_ = message::create();
    timeout();
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

        auto id = uuid();
        ctx->fd = id;
        acceptors_.emplace(id, ctx);
        return id;
    }
    catch (asio::system_error& e)
    {
        CONSOLE_ERROR(router_->logger(), "%s:%d %s(%d)", host.data(), port, e.what(), e.code().value());
        return 0;
    }
}

void socket::accept(int fd, int32_t sessionid, uint32_t owner)
{
    MOON_CHECK(owner > 0, "socket::accept : invalid serviceid");
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

    worker* w = router_->get_worker(router_->worker_id(owner));
    auto c = w->socket().make_connection(owner, ctx->type);

    ctx->acceptor.async_accept(c->socket(), [this, ctx, c, w, sessionid, owner](const asio::error_code& e)
    {
        if (!e)
        {
            c->fd(w->socket().uuid());
            w->socket().add_connection(this, ctx, c, sessionid);
        }
        else
        {
            if (sessionid != 0)
            {
                response(ctx->fd, ctx->owner, moon::format("socket::accept error %s(%d)", e.message().data(), e.value()), "error"sv, sessionid, PTYPE_ERROR);
            }
            else
            {
                if (e != asio::error::operation_aborted)
                {
                    CONSOLE_WARN(router_->logger(), "socket::accept error %s(%d)", e.message().data(), e.value());
                }
            }
        }

        if (sessionid == 0)
        {
            accept(ctx->fd, sessionid, owner);
        }
    });
}

int socket::connect(const std::string& host, uint16_t port, uint32_t owner, uint8_t type, int32_t sessionid, int32_t timeout)
{
    try
    {
        asio::ip::tcp::resolver resolver(ioc_);
        asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));

        auto c = make_connection(owner, type);

        if (0 == sessionid)
        {
            asio::connect(c->socket(), endpoints);
            c->fd(uuid());
            connections_.emplace(c->fd(), c);
            c->start(false);
            return c->fd();
        }
        else
        {
            if (timeout > 0)
            {
                std::shared_ptr<asio::steady_timer> connect_timer = std::make_shared<asio::steady_timer>(ioc_);
                connect_timer->expires_after(std::chrono::milliseconds(timeout));
                connect_timer->async_wait([this, c, owner, sessionid, host, port, connect_timer](const asio::error_code & e) {
                    if (e)
                    {
                        CONSOLE_ERROR(router_->logger(), "connect %s:%d timer error %s", host.data(), port, e.message().data());
                        return;
                    }
                    if (c->fd() == 0)
                    {
                        c->close();
                        response(0, owner, std::string_view{}, moon::format("connect %s:%d timeout", host.data(), port), sessionid, PTYPE_ERROR);
                    }
                });
            }

            asio::async_connect(c->socket(), endpoints,
                [this, c, host, port, owner, sessionid](const asio::error_code& e, const asio::ip::tcp::endpoint&)
            {
                if (!e)
                {
                    c->fd(uuid());
                    connections_.emplace(c->fd(), c);
                    c->start(false);
                    response(0, owner, std::to_string(c->fd()), std::string_view{}, sessionid, PTYPE_TEXT);
                }
                else
                {
                    if (c->socket().is_open())
                    {
                        response(0, owner, std::string_view{}, moon::format("connect %s:%d failed: %s(%d)", host.data(), port, e.message().data(), e.value()), sessionid, PTYPE_ERROR);
                    }
                }
            });
        }
    }
    catch (asio::system_error& e)
    {
        if (sessionid == 0)
        {
            CONSOLE_WARN(router_->logger(), "connect %s:%d failed: %s(%d)", host.data(), port, e.code().message().data(), e.code().value());
        }
        else
        {
            asio::post(ioc_, [this, host, port, owner, sessionid, e]() {
                response(0, owner, std::string_view{}
                    , moon::format("connect %s:%d failed: %s(%d)", host.data(), port, e.code().message().data(), e.code().value())
                    , sessionid, PTYPE_ERROR);
            });
        }
    }
    return 0;
}

void socket::read(uint32_t fd, uint32_t owner, size_t n, read_delim delim, int32_t sessionid)
{
    do
    {
        if (auto iter = connections_.find(fd); iter != connections_.end())
        {
            if (iter->second->read(moon::read_request{ delim, n, sessionid }))
            {
                return;
            }
        }
    } while (0);
    asio::post(ioc_, [this, owner, sessionid]() {
        response(0, owner, "read an invalid socket", "closed", sessionid, PTYPE_ERROR);
    });
}

bool socket::write(uint32_t fd, buffer_ptr_t data)
{
    auto iter = connections_.find(fd);
    if (iter == connections_.end())
    {
        return false;
    }
    return iter->second->send(std::move(data));
}

bool socket::write_with_flag(uint32_t fd, buffer_ptr_t data, int flag)
{
    auto iter = connections_.find(fd);
    if (iter == connections_.end())
    {
        return false;
    }
    MOON_ASSERT(flag > 0 && flag < static_cast<int>(buffer_flag::buffer_flag_max), "socket::write_with_flag flag invalid");
    data->set_flag(static_cast<buffer_flag>(flag));
    return iter->second->send(std::move(data));
}

bool socket::write_message(uint32_t fd, message * m)
{
    return write(fd, *m);
}

bool socket::close(uint32_t fd, bool remove)
{
    if (auto iter = connections_.find(fd); iter != connections_.end())
    {
        iter->second->close();
        if (remove)
        {
            connections_.erase(iter);
            unlock_fd(fd);
        }
        return true;
    }

    if (auto iter = acceptors_.find(fd); iter != acceptors_.end())
    {
        if (iter->second->acceptor.is_open())
        {
            iter->second->acceptor.cancel();
            iter->second->acceptor.close();
        }
        if (remove)
        {
            acceptors_.erase(iter);
            unlock_fd(fd);
        }
        return true;
    }
    return false;
}

bool socket::settimeout(uint32_t fd, int v)
{
    if (auto iter = connections_.find(fd); iter != connections_.end())
    {
        iter->second->settimeout(v);
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

bool socket::set_enable_frame(uint32_t fd, std::string flag)
{
    moon::lower(flag);
    moon::frame_enable_flag v = frame_enable_flag::none;
    switch (moon::chash_string(flag))
    {
    case "none"_csh:
    {
        v = moon::frame_enable_flag::none;
        break;
    }
    case "r"_csh:
    {
        v = moon::frame_enable_flag::receive;
        break;
    }
    case "w"_csh:
    {
        v = moon::frame_enable_flag::send;
        break;
    }
    case "wr"_csh:
    case "rw"_csh:
    {
        v = moon::frame_enable_flag::both;
        break;
    }
    default:
        CONSOLE_WARN(router_->logger(), "tcp::set_enable_frame Unsupported  enable frame flag %s.Support: 'r' 'w' 'wr' 'rw'.", flag.data());
        return false;
    }

    if (auto iter = connections_.find(fd); iter != connections_.end())
    {
        auto c = std::dynamic_pointer_cast<moon_connection>(iter->second);
        if (c)
        {
            c->set_frame_flag(v);
            return true;
        }
    }
    return false;
}

bool moon::socket::set_send_queue_limit(uint32_t fd, uint32_t warnsize, uint32_t errorsize)
{
    if (auto iter = connections_.find(fd); iter != connections_.end())
    {
        iter->second->set_send_queue_limit(warnsize, errorsize);
        return true;
    }
    return false;
}

size_t moon::socket::socket_num()
{
    return connections_.size();
}

std::string moon::socket::getaddress(uint32_t fd)
{
	if (auto iter = connections_.find(fd); iter != connections_.end())
	{
		return iter->second->address();
	}
	return std::string();
}

uint32_t socket::uuid()
{
    uint32_t res = 0;
    do
    {
        res = uuid_.fetch_add(1);
        res %= max_socket_num;
        ++res;
        res |= (worker_->id() << 16);
    } while (!try_lock_fd(res));
    return res;
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
    connection->logger(router_->logger());
    return connection;
}

void socket::response(uint32_t sender, uint32_t receiver, string_view_t data, string_view_t header, int32_t sessionid, uint8_t type)
{
    if (0 == sessionid)
        return;
    response_->set_sender(sender);
    response_->set_receiver(0);
    response_->get_buffer()->clear();
    response_->get_buffer()->write_back(data.data(), data.size());
    response_->set_header(header);
    response_->set_sessionid(sessionid);
    response_->set_type(type);

    handle_message(receiver, response_);
}

bool socket::try_lock_fd(uint32_t fd)
{
    std::unique_lock lck(lock_);
    return fd_watcher_.emplace(fd).second;
}

void socket::unlock_fd(uint32_t fd)
{
    std::unique_lock lck(lock_);
    size_t count = fd_watcher_.erase(fd);
    MOON_CHECK(count == 1, "socket fd erase failed!");
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
