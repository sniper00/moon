#include "socket_server.h"
#include "common/hash.hpp"
#include "common/string.hpp"
#include "server.h"
#include "worker.h"

#include "network/moon_connection.hpp"
#include "network/stream_connection.hpp"
#include "network/ws_connection.hpp"

using namespace moon;

socket_server::socket_server(server* s, worker* w, asio::io_context& ioctx):
    server_(s),
    worker_(w),
    context_(ioctx),
    timer_(ioctx) {
    timeout();
}

bool socket_server::try_open(const std::string& host, uint16_t port, bool is_connect) {
    try {
        tcp::resolver resolver { context_ };
        if (is_connect) {
            tcp::socket sock(context_);
            asio::connect(sock, resolver.resolve(host, std::to_string(port)));
            sock.close();
            return true;
        } else {
            tcp::endpoint endpoint = *resolver.resolve(host, std::to_string(port)).begin();
            tcp::acceptor acceptor { context_ };
            acceptor.open(endpoint.protocol());
#if TARGET_PLATFORM != PLATFORM_WINDOWS
            acceptor.set_option(tcp::acceptor::reuse_address(true));
#endif
            acceptor.bind(endpoint);
            acceptor.close();
            return true;
        }
    } catch (const asio::system_error& e) {
        CONSOLE_ERROR("%s:%d %s(%d)", host.data(), port, e.what(), e.code().value());
        return false;
    }
}

std::pair<uint32_t, tcp::endpoint>
socket_server::listen(const std::string& host, uint16_t port, uint32_t owner, uint8_t type) {
    try {
        auto ctx = std::make_shared<socket_server::acceptor_context>(type, owner, context_);
        tcp::resolver resolver(context_);
        tcp::endpoint endpoint = *resolver.resolve(host, std::to_string(port)).begin();
        ctx->acceptor.open(endpoint.protocol());
#if TARGET_PLATFORM != PLATFORM_WINDOWS
        ctx->acceptor.set_option(tcp::acceptor::reuse_address(true));
#endif
        ctx->acceptor.bind(endpoint);
        ctx->acceptor.listen(std::numeric_limits<int>::max());
        ctx->reset_reserve();
        auto id = server_->nextfd();
        ctx->fd = id;
        acceptors_.try_emplace(id, ctx);
        return std::make_pair(id, ctx->acceptor.local_endpoint());
    } catch (const asio::system_error& e) {
        CONSOLE_ERROR("%s:%d %s(%d)", host.data(), port, e.what(), e.code().value());
        return { 0, tcp::endpoint {} };
    }
}

uint32_t socket_server::udp_open(uint32_t owner, std::string_view host, uint16_t port) {
    try {
        udp_context_ptr_t ctx;
        if (host.empty()) {
            ctx = std::make_shared<socket_server::udp_context>(owner, context_);
        } else {
            udp::resolver resolver(context_);
            udp::endpoint endpoint = *resolver.resolve(host, std::to_string(port)).begin();
            ctx = std::make_shared<socket_server::udp_context>(owner, context_, endpoint);
        }
        auto id = server_->nextfd();
        ctx->fd = id;
        do_receive(ctx);
        udp_.try_emplace(id, std::move(ctx));
        return id;
    } catch (const asio::system_error& e) {
        CONSOLE_ERROR("%s:%d %s(%d)", host.data(), port, e.what(), e.code().value());
        return 0;
    }
}

bool socket_server::udp_connect(uint32_t fd, std::string_view host, uint16_t port) {
    try {
        if (auto iter = udp_.find(fd); iter != udp_.end()) {
            udp::resolver resolver(context_);
            udp::endpoint endpoint = *resolver.resolve(host, std::to_string(port)).begin();
            iter->second->sock.connect(endpoint);
            return true;
        }
        return false;
    } catch (const asio::system_error& e) {
        CONSOLE_ERROR("%s:%d %s(%d)", host.data(), port, e.what(), e.code().value());
        return false;
    }
}

bool socket_server::accept(uint32_t fd, int64_t sessionid, uint32_t owner) {
    auto iter = acceptors_.find(fd);
    if (iter == acceptors_.end()) {
        return false;
    }

    auto const& ctx = iter->second;
    if (!ctx->acceptor.is_open()) {
        return false;
    }

    worker* w = server_->get_worker(0, owner);
    if (nullptr == w)
        return false;

    auto c = w->socket_server().make_connection(owner, ctx->type, tcp::socket(w->io_context()));

    ctx->acceptor.async_accept(
        c->socket(),
        [this, ctx, c, w, sessionid, owner](const asio::error_code& e) {
            if (!e) {
                if (!ctx->reserve.is_open()) {
                    c->socket().close();
                    ctx->reset_reserve();
                    auto ec = asio::error::make_error_code(asio::error::no_descriptors);
                    response(
                        ctx->fd,
                        ctx->owner,
                        moon::format(
                            "socket_server::accept %s(%d)",
                            ec.message().data(),
                            ec.value()
                        ),
                        sessionid,
                        PTYPE_ERROR
                    );
                } else {
                    c->fd(server_->nextfd());
                    w->socket_server().add_connection(this, ctx, c, sessionid);
                }
            } else {
                if (e == asio::error::operation_aborted)
                    return;

                if (e == asio::error::no_descriptors && ctx->reserve.is_open()) {
                    ctx->reserve.close();
                }

                response(
                    ctx->fd,
                    ctx->owner,
                    moon::format("socket_server::accept %s(%d)", e.message().data(), e.value()),
                    sessionid,
                    PTYPE_ERROR
                );
            }

            if (sessionid == 0) {
                accept(ctx->fd, sessionid, owner);
            }
        }
    );
    return true;
}

void socket_server::connect(
    const std::string& host,
    uint16_t port,
    uint32_t owner,
    uint8_t type,
    int64_t sessionid,
    uint32_t millseconds
) {
    struct connect_params {
        uint16_t port;
        uint32_t owner;
        int64_t millseconds;
        int64_t sessionid;
        std::string host;
    };

    auto params = std::make_shared<connect_params>(
        connect_params { port, owner, millseconds, sessionid, host }
    );

    auto conn = make_connection(owner, type, tcp::socket(context_));

    if (millseconds > 0) {
        auto timer = std::make_unique<asio::steady_timer>(context_);
        timer->expires_after(std::chrono::milliseconds(millseconds));
        timer->async_wait([this, params, conn, _ = std::move(timer)](const asio::error_code& ec) {
            if (ec)
                return;
            if (conn->fd() == 0) {
                conn->close();
                response(
                    0,
                    params->owner,
                    moon::format("connect %s:%d timeout", params->host.data(), params->port),
                    params->sessionid,
                    PTYPE_ERROR
                );
            }
        });
    }

    auto resolver = std::make_unique<tcp::resolver>(context_);
    resolver->async_resolve(
        host,
        std::to_string(port),
        [this, params, resolver = std::move(resolver), conn = std::move(conn)](
            const asio::error_code& ec,
            tcp::resolver::results_type results
        ) mutable {
            if (params->millseconds > 0 && conn.use_count() == 1) // Already timed out
                return;

            if (!ec) {
                auto& socket = conn->socket();
                asio::async_connect(
                    socket,
                    results,
                    [this,
                     params,
                     conn = std::move(conn)](const asio::error_code& ec, const tcp::endpoint&) {
                        if (params->millseconds > 0 && conn.use_count() == 1) // Already timed out
                            return;

                        if (!ec) {
                            conn->fd(server_->nextfd());
                            connections_.try_emplace(conn->fd(), conn);
                            conn->start(base_connection::role::client);
                            response(
                                0,
                                params->owner,
                                std::to_string(conn->fd()),
                                params->sessionid,
                                PTYPE_INTEGER
                            );
                        } else {
                            //Set the fd flag to prevent timeout handling
                            conn->fd(std::numeric_limits<uint32_t>::max()); 
                            response(
                                0,
                                params->owner,
                                moon::format(
                                    "connect %s:%d %s(%d)",
                                    params->host.data(),
                                    params->port,
                                    ec.message().data(),
                                    ec.value()
                                ),
                                params->sessionid,
                                PTYPE_ERROR
                            );
                        }
                    }
                );
            } else {
                //Set the fd flag to prevent timeout handling
                conn->fd(std::numeric_limits<uint32_t>::max());
                response(
                    0,
                    params->owner,
                    moon::format(
                        "resolve %s:%d %s(%d)",
                        params->host.data(),
                        params->port,
                        ec.message().data(),
                        ec.value()
                    ),
                    params->sessionid,
                    PTYPE_ERROR
                );
            }
        }
    );
}

direct_read_result
socket_server::read(uint32_t fd, size_t n, std::string_view delim, int64_t sessionid) {
    if (auto iter = connections_.find(fd); iter != connections_.end()) {
        return iter->second->read(n, delim, sessionid);
    }
    return direct_read_result { false, { "socket.read: closed" } };
}

bool socket_server::write(uint32_t fd, buffer_shr_ptr_t data, socket_send_mask mask) {
    if (nullptr == data || 0 == data->size())
        return false;

    if (auto iter = connections_.find(fd); iter != connections_.end()) {
        data->add_bitmask(mask);
        return iter->second->send(std::move(data));
    }

    if (auto iter = udp_.find(fd); iter != udp_.end()) {
        auto buf = asio::buffer(data->data(), data->size());
        iter->second->sock.async_send(
            buf,
            [_ = std::move(data),
             ctx = iter->second](std::error_code /*ec*/, std::size_t /*bytes_sent*/) {
                //do nothing
            }
        );
        return true;
    }
    return false;
}

bool socket_server::close(uint32_t fd) {
    if (auto iter = connections_.find(fd); iter != connections_.end()) {
        iter->second->close();
        connections_.erase(iter);
        server_->unlock_fd(fd);
        return true;
    }

    if (auto iter = udp_.find(fd); iter != udp_.end()) {
        iter->second->closed = true;
        iter->second->sock.close();
        udp_.erase(iter);
        server_->unlock_fd(fd);
        return true;
    }

    if (auto iter = acceptors_.find(fd); iter != acceptors_.end()) {
        iter->second->close();
        acceptors_.erase(iter);
        server_->unlock_fd(fd);
        return true;
    }
    return false;
}

void socket_server::close_all() const {
    for (const auto& [_, v]: connections_) {
        v->close();
    }

    for (const auto& [_, v]: udp_) {
        v->sock.close();
    }

    for (const auto& [_, v]: acceptors_) {
        v->close();
    }
}

bool socket_server::settimeout(uint32_t fd, uint32_t seconds) {
    if (auto iter = connections_.find(fd); iter != connections_.end()) {
        iter->second->settimeout(seconds);
        return true;
    }
    return false;
}

bool socket_server::setnodelay(uint32_t fd) {
    if (auto iter = connections_.find(fd); iter != connections_.end()) {
        return iter->second->set_no_delay();
    }
    return false;
}

bool socket_server::set_enable_chunked(uint32_t fd, std::string_view flag) {
    auto v = enable_chunked::none;
    for (const auto& c: flag) {
        switch (c) {
            case 'r':
            case 'R':
                v = v | moon::enable_chunked::receive;
                break;
            case 'w':
            case 'W':
                v = v | moon::enable_chunked::send;
                break;
            default:
                CONSOLE_WARN(
                    "tcp::set_enable_chunked Unsupported enable chunked flag %s.Support: 'r' 'w'.",
                    flag.data()
                );
                return false;
        }
    }

    if (auto iter = connections_.find(fd); iter != connections_.end()) {
        auto c = std::dynamic_pointer_cast<moon_connection>(iter->second);
        if (c) {
            c->set_enable_chunked(v);
            return true;
        }
    }
    return false;
}

bool socket_server::set_send_queue_limit(uint32_t fd, uint16_t warnsize, uint16_t errorsize) {
    if (auto iter = connections_.find(fd); iter != connections_.end()) {
        iter->second->set_send_queue_limit(warnsize, errorsize);
        return true;
    }
    return false;
}

static bool decode_endpoint(std::string_view address, udp::endpoint& ep) {
    if (address[0] != '4' && address[0] != '6')
        return false;
    port_type port = 0;
    if (address[0] == '4') {
        address = address.substr(1);
        address_v4::bytes_type bytes;
        if ((address.size()) != bytes.size() + sizeof(port))
            return false;
        memcpy(bytes.data(), address.data(), bytes.size());
        memcpy(&port, address.data() + bytes.size(), sizeof(port));
        ep = udp::endpoint(asio::ip::address(make_address_v4(bytes)), port);
    } else {
        address = address.substr(1);
        address_v6::bytes_type bytes;
        if ((address.size()) != bytes.size() + sizeof(port))
            return false;
        memcpy(bytes.data(), address.data(), bytes.size());
        memcpy(&port, address.data() + bytes.size(), sizeof(port));
        ep = udp::endpoint(asio::ip::address(make_address_v6(bytes)), port);
    }
    return true;
}

bool socket_server::send_to(uint32_t host, std::string_view address, buffer_shr_ptr_t data) {
    if (address.empty())
        return false;
    if (auto iter = udp_.find(host); iter != udp_.end()) {
        udp::endpoint ep;
        if (!decode_endpoint(address, ep))
            return false;

        auto buf = asio::buffer(data->data(), data->size());
        iter->second->sock.async_send_to(
            buf,
            ep,
            [_ = std::move(data),
             ctx = iter->second](std::error_code /*ec*/, std::size_t /*bytes_sent*/) {
                //do nothing
            }
        );
        return true;
    }
    return false;
}

std::string moon::socket_server::getaddress(uint32_t fd) {
    if (auto iter = connections_.find(fd); iter != connections_.end()) {
        return iter->second->address();
    }
    return std::string();
}

bool moon::socket_server::switch_type(uint32_t fd, uint8_t new_type) {
    if (auto iter = connections_.find(fd); iter != connections_.end()) {
        auto c = iter->second;
        if (c->type() != PTYPE_SOCKET_TCP)
            return false;
        auto newc = make_connection(c->owner(), new_type, std::move(c->socket()));
        newc->fd(fd);
        iter->second = newc;
        newc->start(c->get_role());
        return true;
    }
    return false;
}

std::array<char, socket_server::addr_v6_size>
socket_server::encode_endpoint(const address& addr, port_type port) {
    std::array<char, socket_server::addr_v6_size> buf {};
    size_t size = 0;
    if (addr.is_v4()) {
        buf[0] = '4';
        size += 1;
        auto bytes = addr.to_v4().to_bytes();
        memcpy(buf.data() + size, bytes.data(), bytes.size());
        size += bytes.size();
    } else if (addr.is_v6()) {
        buf[0] = '6';
        size += 1;
        auto bytes = addr.to_v6().to_bytes();
        memcpy(buf.data() + size, bytes.data(), bytes.size());
        size += bytes.size();
    }
    memcpy(buf.data() + size, &port, sizeof(port));
    return buf;
}

connection_ptr_t
socket_server::make_connection(uint32_t serviceid, uint8_t type, tcp::socket&& sock) {
    connection_ptr_t connection;
    switch (type) {
        case PTYPE_SOCKET_MOON: {
            connection = std::make_shared<moon_connection>(serviceid, type, this, std::move(sock));
            break;
        }
        case PTYPE_SOCKET_TCP: {
            connection =
                std::make_shared<stream_connection>(serviceid, type, this, std::move(sock));
            break;
        }
        case PTYPE_SOCKET_WS: {
            connection = std::make_shared<ws_connection>(serviceid, type, this, std::move(sock));
            break;
        }
        default:
            MOON_ASSERT(false, "Unknown socket protocol");
            break;
    }
    return connection;
}

void socket_server::response(
    uint32_t sender,
    uint32_t receiver,
    std::string_view content,
    int64_t sessionid,
    uint8_t type
) {
    if (0 == sessionid) {
        CONSOLE_ERROR("%s", std::string(content).data());
        return;
    }
    response_.set_sender(sender);
    response_.set_receiver(0);
    response_.as_buffer()->clear();
    response_.write_data(content);
    response_.set_sessionid(sessionid);
    response_.set_type(type);

    handle_message(receiver, response_);
}

void socket_server::add_connection(
    socket_server* from,
    const acceptor_context_ptr_t& ctx,
    const connection_ptr_t& c,
    int64_t sessionid
) {
    asio::dispatch(context_, [this, from, ctx, c, sessionid] {
        connections_.try_emplace(c->fd(), c);
        c->start(base_connection::role::server);

        if (sessionid != 0) {
            asio::dispatch(from->context_, [from, ctx, sessionid, fd = c->fd()] {
                from->response(ctx->fd, ctx->owner, std::to_string(fd), sessionid, PTYPE_INTEGER);
            });
        }
    });
}

service* socket_server::find_service(uint32_t serviceid) {
    return worker_->find_service(serviceid);
}

void socket_server::timeout() {
    timer_.expires_after(std::chrono::seconds(10));
    timer_.async_wait([this](const asio::error_code& e) {
        if (e) {
            return;
        }

        auto now = base_connection::now();
        for (const auto& [_, v]: connections_) {
            v->timeout(now);
        }
        timeout();
    });
}

void socket_server::do_receive(const udp_context_ptr_t& ctx) {
    if (ctx->closed)
        return;

    auto buf = ctx->msg.as_buffer();
    buf->clear();
    //reserve addr_v6_size bytes for address
    buf->commit(addr_v6_size);
    buf->seek(addr_v6_size, buffer::seek_origin::Begin);

    auto [k, v] = buf->writeable();
    ctx->sock.async_receive_from(
        asio::buffer(k, v),
        ctx->from_ep,
        [this, ctx](std::error_code ec, std::size_t bytes_recvd) {
            if (!ec && bytes_recvd > 0) {
                auto b = ctx->msg.as_buffer();
                b->commit(bytes_recvd);
                auto bytes = encode_endpoint(ctx->from_ep.address(), ctx->from_ep.port());
                b->write_front(bytes.data(), bytes.size());
                ctx->msg.set_sender(ctx->fd);
                ctx->msg.set_receiver(0);
                ctx->msg.set_type(PTYPE_SOCKET_UDP);
                handle_message(ctx->owner, ctx->msg);
            }
            do_receive(ctx);
        }
    );
}
