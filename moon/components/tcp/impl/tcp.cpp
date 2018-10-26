/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#include "components/tcp/tcp.h"
#include "common/log.hpp"
#include "common/string.hpp"
#include "message.hpp"
#include "moon_connection.hpp"
#include "custom_connection.hpp"
#include "ws_connection.hpp"

namespace moon
{
    tcp::tcp() noexcept
        :io_ctx_(nullptr)
        , connuid_(1)
        , timeout_(0)
        , type_(protocol_type::protocol_default)
        , frame_flag_(frame_enable_flag::none)
        , parent_(nullptr)
    {
    }

    tcp::~tcp()
    {

    }

    void tcp::setprotocol(std::string flag)
    {
        moon::lower(flag);
        switch (moon::chash_string(flag.data()))
        {
        case moon::chash_string("default"):
        {
            type_ = moon::protocol_type::protocol_default;
            break;
        }
        case moon::chash_string("custom"):
        {
            type_ = moon::protocol_type::protocol_custom;
            break;
        }
        case moon::chash_string("websocket"):
        {
            type_ = moon::protocol_type::protocol_websocket;
            break;
        }
        default:
            CONSOLE_WARN(logger(), "tcp::setprotocol Unsupported  protocol %s.Support: 'default' 'custom' 'websocket'.", flag.data());
            break;
        }
    }

    void tcp::settimeout(int seconds)
    {
        checker_ = std::make_unique<asio::steady_timer>(io_context());
        timeout_ = seconds;
        check();
    }

    void tcp::setnodelay(uint32_t connid)
    {
        do
        {
            auto iter = conns_.find(connid);
            if (iter == conns_.end())
                break;
            iter->second->set_no_delay();
        } while (0);
    }

    void tcp::set_enable_frame(std::string flag)
    {
        moon::lower(flag);
        switch (moon::chash_string(flag.data()))
        {
        case moon::chash_string("none"):
        {
            frame_flag_ = moon::frame_enable_flag::none;
            break;
        }
        case moon::chash_string("r"):
        {
            frame_flag_ = moon::frame_enable_flag::receive;
            break;
        }
        case moon::chash_string("w"):
        {
            frame_flag_ = moon::frame_enable_flag::send;
            break;
        }
        case moon::chash_string("wr"):
        case moon::chash_string("rw"):
        {
            frame_flag_ = moon::frame_enable_flag::both;
            break;
        }
        default:
            CONSOLE_WARN(logger(), "tcp::set_enable_frame Unsupported  enable frame flag %s.Support: 'r' 'w' 'wr' 'rw'.", flag.data());
            break;
        }
    }

    bool tcp::listen(const std::string & ip, const std::string & port)
    {
        try
        {
            acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(io_context());
            asio::ip::tcp::resolver resolver(acceptor_->get_io_service());
            asio::ip::tcp::resolver::query query(ip, port);
            resolver.resolve(query);
            auto iter = resolver.resolve(query);
            asio::ip::tcp::endpoint endpoint = *iter;
            acceptor_->open(endpoint.protocol());
#if TARGET_PLATFORM != PLATFORM_WINDOWS
            acceptor_->set_option(asio::ip::tcp::acceptor::reuse_address(true));
#endif
            acceptor_->bind(endpoint);
            acceptor_->listen(std::numeric_limits<int>::max());
            if (type_ == protocol_type::protocol_default || type_ == protocol_type::protocol_websocket)
            {
                async_accept(0);
            }
            return true;
        }
        catch (asio::system_error& e)
        {
            CONSOLE_ERROR(logger(), "%s:%s %s(%d)",ip.data(),port.data(), e.what(), e.code().value());
            return false;
        }
    }

    void tcp::async_accept(int32_t responseid)
    {
        if (!acceptor_->is_open())
            return;

        auto conn = create_connection();
        acceptor_->async_accept(conn->socket(), [this, self = shared_from_this(), conn, responseid](const asio::error_code& e)
        {
            if (!self->ok())
            {
                return;
            }

            if (!e)
            {
                conn->set_id(make_connid());
                conns_.emplace(conn->id(), conn);
                conn->start(true);

                switch (type_)
                {
                case protocol_type::protocol_default:
                case protocol_type::protocol_websocket:
                    async_accept(0);
                    break;
                case protocol_type::protocol_custom:
                    make_response(std::to_string(conn->id()), "", responseid, PTYPE_TEXT);
                    break;
                default:
                    break;
                }
            }
            else
            {
                if (type_ == protocol_type::protocol_default || type_ == protocol_type::protocol_websocket)
                {
                    CONSOLE_WARN(logger(), "tcp async_accept error %s(%d)", e.message().data(), e.value());
                }
                else
                {
                    make_response(moon::format("tcp async_accept error %s(%d)", e.message().data(), e.value()), "error", responseid, PTYPE_ERROR);
                }
            }
        });
    }

    void tcp::async_connect(const std::string & ip, const std::string & port, int32_t responseid)
    {
        try
        {
            asio::ip::tcp::resolver resolver(io_context());
            asio::ip::tcp::resolver::query query(ip, port);
            asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
            auto conn = create_connection();
            asio::async_connect(conn->socket(), endpoint_iterator,
                [this, self = shared_from_this(), conn, ip, port, responseid](const asio::error_code& e, asio::ip::tcp::resolver::iterator)
            {
                if (!self->ok())
                {
                    return;
                }

                if (!e)
                {
                    conn->set_id(make_connid());
                    conns_.emplace(conn->id(), conn);
                    conn->start(false);
                    make_response(std::to_string(conn->id()), "", responseid, PTYPE_TEXT);
                }
                else
                {
                    make_response(moon::format("tcp async_connect error %s(%d)", e.message().data(), e.value()), "error", responseid, PTYPE_ERROR);
                }
            });
        }
        catch (asio::system_error& e)
        {
            make_response(moon::format("tcp async_connect error %s(%d)", e.what(), e.code().value())
                , "error"
                , responseid, PTYPE_ERROR);
        }
    }

    uint32_t tcp::connect(const std::string & ip, const std::string & port)
    {
        try
        {
            asio::ip::tcp::resolver resolver(io_context());
            asio::ip::tcp::resolver::query query(ip, port);
            asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
            auto conn = create_connection();
            asio::connect(conn->socket(), endpoint_iterator);
            conn->set_id(make_connid());
            conns_.emplace(conn->id(), conn);
            conn->start(false);
            return conn->id();
        }
        catch (asio::system_error& e)
        {
            CONSOLE_WARN(logger(), "%s:%s %s(%d)", ip.data(), port.data(), e.what(),e.code().value());
            return 0;
        }
    }

    void tcp::read(uint32_t connid, size_t n, read_delim delim, int32_t responseid)
    {
        do
        {
            auto iter = conns_.find(connid);
            if (iter == conns_.end())
                break;

            if (!iter->second->read(moon::read_request{ delim, n, responseid }))
            {
                break;
            }
            return;
        } while (0);
        io_context().post([this, responseid]() {
            make_response("read a invlid socket", "closed", responseid, PTYPE_ERROR);
        });
    }

    bool tcp::send(uint32_t connid, const buffer_ptr_t & data)
    {
        auto iter = conns_.find(connid);
        if (iter == conns_.end())
        {
            return false;
        }
        return iter->second->send(data);
    }

    bool tcp::send_then_close(uint32_t connid, const buffer_ptr_t & data)
    {
        auto iter = conns_.find(connid);
        if (iter == conns_.end())
        {
            return false;
        }
        data->set_flag(static_cast<uint8_t>(buffer_flag::close));
        return iter->second->send(data);
    }

    bool tcp::send_message(uint32_t connid, message * msg)
    {
        return send(connid, *msg);
    }

    bool tcp::close(uint32_t connid)
    {
        auto iter = conns_.find(connid);
        if (iter == conns_.end())
        {
            return false;
        }
        iter->second->close();
        conns_.erase(connid);
        return true;
    }

    void tcp::init()
    {
        component::init();
        parent_ = parent<service>();
        MOON_DCHECK(parent_ != nullptr, "tcp::init service is null");
        io_ctx_ = &(parent_->get_router()->get_io_context(parent_->id()));
        response_msg_ = message::create();
    }

    void tcp::destroy()
    {
        component::destroy();

        for (auto& conn : conns_)
        {
            conn.second->close(true);
        }

        if (checker_ != nullptr)
        {
            checker_->cancel();
        }

        if (nullptr != acceptor_ && acceptor_->is_open())
        {
            acceptor_->cancel();
            acceptor_->close();
        }
    }

    void tcp::handle_message(const message_ptr_t & msg)
    {
        if (!ok())
        {
            return;
        }
        msg->set_receiver(parent_->id());
        parent_->handle_message(msg);
        if (msg->type() == PTYPE_ERROR || msg->subtype() == static_cast<uint8_t>(socket_data_type::socket_close))
        {
            conns_.erase(msg->sender());
        }
    }

    void tcp::check()
    {
        checker_->expires_from_now(std::chrono::seconds(10));
        checker_->async_wait([this, self = shared_from_this()](const asio::error_code & e) {
            if (e || !self->ok())
            {
                return;
            }
            auto now = std::time(nullptr);
            for (auto& conn : conns_)
            {
                conn.second->timeout_check(now, timeout_);
            }
            check();
        });
    }

    asio::io_context & tcp::io_context()
    {
        return *io_ctx_;
    }

    uint32_t tcp::make_connid()
    {
        if (connuid_ == 0xFFFF)
            connuid_ = 1;
        auto id = connuid_++;
        while (conns_.find(id) != conns_.end())
        {
            id = connuid_++;
        }
        return id;
    }
    void tcp::make_response(string_view_t data, string_view_t header, int32_t responseid, uint8_t mtype)
    {
        if (0 == responseid)
            return;
        response_msg_->set_receiver(parent_->id());
        response_msg_->get_buffer()->clear();
        response_msg_->get_buffer()->write_back(data.data(), 0, data.size());
        response_msg_->set_header(header);
        response_msg_->set_responseid(responseid);
        response_msg_->set_type(mtype);

        handle_message(response_msg_);
    }

    tcp::connection_ptr_t tcp::create_connection()
    {
        connection_ptr_t conn;
        switch (type_)
        {
        case moon::protocol_type::protocol_default:
        {
            conn = std::make_shared<moon_connection>(this, io_context());
            break;
        }
        case moon::protocol_type::protocol_custom:
            conn = std::make_shared<custom_connection>(this, io_context());
            break;
        case moon::protocol_type::protocol_websocket:
            conn = std::make_shared<ws_connection>(this, io_context());
            break;
        default:
            break;
        }
        conn->logger(this->logger());
        conn->set_enable_frame(frame_flag_);
        return conn;
    }
}


