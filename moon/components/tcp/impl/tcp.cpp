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
#include "service.h"
#include "core/worker.h"
#include "moon_connection.hpp"
#include "custom_connection.hpp"
#include "ws_connection.hpp"
#include "router.h"

namespace moon
{

    using connection_ptr_t = std::shared_ptr<base_connection>;

    struct tcp::imp
    {
        imp() noexcept
            : connuid_(1)
            , timeout_(0)
            , type_(protocol_type::protocol_default)
            , frame_type_(frame_enable_type::none)
            , parent_(nullptr)
        {
        }

        asio::io_service& io_service()
        {
            return *ios_;
        }

        uint32_t make_connid()
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

        void make_response(string_view_t data, const std::string& header, int32_t responseid, uint8_t mtype = PTYPE_SOCKET)
        {
            if (0 == responseid)
                return;
			response_msg_->set_receiver(parent_->id());
            response_msg_->get_buffer()->clear();
            response_msg_->get_buffer()->write_back(data.data(), 0, data.size());
            response_msg_->set_header(header);
            response_msg_->set_responseid(-responseid);
            response_msg_->set_type(mtype);
            parent_->handle_message(response_msg_);
        }

        connection_ptr_t create_connection(tcp* t)
        {
            connection_ptr_t conn;
            switch (type_)
            {
            case moon::protocol_type::protocol_default:
                conn = std::make_shared<moon_connection>(io_service(),t);
                break;
            case moon::protocol_type::protocol_custom:
                conn = std::make_shared<custom_connection>(io_service(),t);
                break;
            case moon::protocol_type::protocol_websocket:
                conn = std::make_shared<ws_connection>(io_service(),t);
                break;
            default:
                break;
            }
            conn->logger(t->logger());
            return conn;
        }

        std::shared_ptr<tcp> get_self()
        {
            return self_.lock();
        }

        asio::io_service* ios_;
        uint32_t connuid_;
        uint32_t timeout_;
        protocol_type type_;
        frame_enable_type frame_type_;
        service* parent_;
        std::shared_ptr<asio::ip::tcp::acceptor> acceptor_;
        std::shared_ptr<asio::steady_timer> checker_;
        std::unordered_map<uint32_t, connection_ptr_t> conns_;
        message_ptr_t  response_msg_;
        std::weak_ptr<tcp> self_;
    };

    tcp::tcp() noexcept
        :imp_(new imp)
    {
    }

    tcp::~tcp()
    {
        SAFE_DELETE(imp_);
    }

    void tcp::setprotocol(std::string flag)
    {
		moon::lower(flag);
		switch (moon::chash_string(flag.data()))
		{
		case moon::chash_string("default"):
		{
			imp_->type_ = moon::protocol_type::protocol_default;
			break;
		}
		case moon::chash_string("custom"):
		{
			imp_->type_ = moon::protocol_type::protocol_custom;
			break;
		}
		case moon::chash_string("websocket"):
		{
			imp_->type_ = moon::protocol_type::protocol_websocket;
			break;
		}
		default:
			CONSOLE_WARN(logger(), "tcp::setprotocol Unsupported  protocol %s.Support: 'default' 'custom' 'websocket'.", flag.data());
			break;
		}
    }

    void tcp::settimeout(int seconds)
    {
        imp_->checker_ = std::make_shared<asio::steady_timer>(imp_->io_service());
        imp_->timeout_ = seconds;
        check();
    }

    void tcp::setnodelay(uint32_t connid)
    {
        do
        {
            auto iter = imp_->conns_.find(connid);
            if (iter == imp_->conns_.end())
                break;
            iter->second->set_no_delay();
        } while (0);
    }

    void tcp::set_enable_frame(std::string flag)
    {
		moon::lower(flag);
		switch (moon::chash_string(flag.data()))
		{
		case moon::chash_string("r"):
		{
			imp_->frame_type_ = moon::frame_enable_type::receive;
			break;
		}
		case moon::chash_string("w"):
		{
			imp_->frame_type_ = moon::frame_enable_type::send;
			break;
		}
		case moon::chash_string("wr"):
		case moon::chash_string("rw"):
		{
			imp_->frame_type_ = moon::frame_enable_type::both;
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
            imp_->acceptor_ = std::make_shared<asio::ip::tcp::acceptor>(imp_->io_service());

            auto& acceptor_ = imp_->acceptor_;
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
            if (imp_->type_ == protocol_type::protocol_default || imp_->type_ == protocol_type::protocol_websocket)
            {
                async_accept(0);
            }
            return true;
        }
        catch (asio::system_error& e)
        {
            CONSOLE_WARN(logger(), "tcp bind error %s(%d )", e.what(), e.code().value());
            return false;
        }
    }

    void tcp::async_accept(int32_t responseid)
    {
        responseid = -responseid;

        auto acceptor_ = imp_->acceptor_;

        if (!acceptor_->is_open())
            return;

        auto conn = imp_->create_connection(this);
        acceptor_->async_accept(conn->socket(), [this, self = imp_->get_self(), conn, responseid](const asio::error_code& e)
        {
            if (nullptr == self || !self->ok())
            {
                return;
            }

            if (!e)
            {
                conn->set_enable_frame(imp_->frame_type_);
                conn->set_id(imp_->make_connid());
                imp_->conns_.emplace(conn->id(), conn);
                conn->start(true);
                
                switch (imp_->type_)
                {
                case protocol_type::protocol_default:
                case protocol_type::protocol_websocket:
                    async_accept(0);
                    break;
                case protocol_type::protocol_custom:
                    imp_->make_response(std::to_string(conn->id()), "", responseid, PTYPE_TEXT);
                    break;
                default:
                    break;
                }
            }
            else
            {
                if (imp_->type_ == protocol_type::protocol_default || imp_->type_ == protocol_type::protocol_websocket)
                {
                    CONSOLE_WARN(logger(), "tcp async_accept error %s(%d )", e.message().data(), e.value());
                }
                else
                {
                    imp_->make_response(moon::format("tcp async_accept error %s(%d )", e.message().data(), e.value()), "error", responseid, PTYPE_ERROR);
                }
            }
        });
    }

    void tcp::async_connect(const std::string & ip, const std::string & port,int32_t responseid)
    {
        responseid = -responseid;
        try
        {
            asio::ip::tcp::resolver resolver(imp_->io_service());
            asio::ip::tcp::resolver::query query(ip, port);
            asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
            auto conn = imp_->create_connection(this);
            asio::async_connect(conn->socket(), endpoint_iterator,
                [this, self = imp_->get_self(), conn, ip, port, responseid](const asio::error_code& e, asio::ip::tcp::resolver::iterator)
            {
                if (nullptr == self || !self->ok())
                {
                    return;
                }

                if (!e)
                {
                    conn->set_enable_frame(imp_->frame_type_);
                    conn->set_id(imp_->make_connid());
                    imp_->conns_.emplace(conn->id(), conn);
                    conn->start(false);
                    imp_->make_response(std::to_string(conn->id()), "", responseid, PTYPE_TEXT);
                }
                else
                {
                    imp_->make_response(moon::format("tcp async_connect error %s(%d )", e.message().data(), e.value()), "error", responseid, PTYPE_ERROR);
                }
            });
        }
        catch (asio::system_error& e)
        {
            imp_->make_response(moon::format("tcp async_connect error %s(%d )", e.what(), e.code().value())
                , "error"
                , responseid, PTYPE_ERROR);
        }
    }

    uint32_t tcp::connect(const std::string & ip, const std::string & port)
    {
        try
        {
            asio::ip::tcp::resolver resolver(imp_->io_service());
            asio::ip::tcp::resolver::query query(ip, port);
            asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

            auto conn = imp_->create_connection(this);
            asio::connect(conn->socket(), endpoint_iterator);
            conn->set_enable_frame(imp_->frame_type_);
            conn->set_id(imp_->make_connid());
            imp_->conns_.emplace(conn->id(), conn);
            conn->start(false);
            return conn->id();
        }
        catch (asio::system_error& e)
        {
            CONSOLE_WARN(logger(), "tcp::connect error %s",  e.what());
            return 0;
        }
    }

    void tcp::read(uint32_t connid, size_t n, read_delim delim,int32_t responseid)
    {
        responseid = -responseid;
        do
        {
            auto iter = imp_->conns_.find(connid);
            if (iter == imp_->conns_.end())
                break;

            if (!iter->second->read(moon::read_request{ delim, n, responseid }))
            {
                break;
            }
            return;
        } while (0);
        imp_->io_service().post([this, responseid]() {
            imp_->make_response("read a invlid socket", "closed", responseid, PTYPE_ERROR);
        });
    }

    bool tcp::send(uint32_t connid, const buffer_ptr_t & data)
    {
        auto iter = imp_->conns_.find(connid);
        if (iter == imp_->conns_.end())
        {
            return false;
        }
        return iter->second->send(data);
    }

    bool tcp::send_then_close(uint32_t connid, const buffer_ptr_t & data)
    {
        auto iter = imp_->conns_.find(connid);
        if (iter == imp_->conns_.end())
        {
            return false;
        }
        data->set_flag(static_cast<uint8_t>(buffer_flag::close));
        return iter->second->send(data);
    }

    bool tcp::send_message(uint32_t connid, message * msg)
    {
        return send(connid,*msg);
    }

    bool tcp::close(uint32_t connid)
    {
        auto iter = imp_->conns_.find(connid);
        if (iter == imp_->conns_.end())
        {
            return false;
        }
        iter->second->close();
        imp_->conns_.erase(connid);
        return true;
    }

    void tcp::init()
    {
        component::init();
        imp_->parent_ = parent<service>();
        MOON_DCHECK(imp_->parent_ != nullptr, "tcp::init service is null");
        imp_->self_ = imp_->parent_->get_component<tcp>(name());
        imp_->ios_ = &(imp_->parent_->get_router()->get_io_service(imp_->parent_->id()));
        imp_->response_msg_ = message::create();
    }

    void tcp::destroy()
    {
        component::destroy();

        for (auto& conn : imp_->conns_)
        {
            conn.second->close(true);
        }

        if (imp_->checker_ != nullptr)
        {
            imp_->checker_->cancel();
        }

        if (nullptr != imp_->acceptor_ && imp_->acceptor_->is_open())
        {
            imp_->acceptor_->cancel();
            imp_->acceptor_->close();
        }
    }

    void tcp::handle_message(const message_ptr_t & msg)
    {
        if (!ok())
        {
            return;
        }
		msg->set_receiver(imp_->parent_->id());
        imp_->parent_->handle_message(msg);
    }

    void tcp::check()
    {
        auto checker_ = imp_->checker_;
        checker_->expires_from_now(std::chrono::seconds(10));
        checker_->async_wait([this, self = imp_->get_self()](const asio::error_code & e) {
            if (e || nullptr == self || !self->ok())
            {
                return;
            }      
            auto now = std::time(nullptr);
            for (auto& conn : imp_->conns_)
            {
                conn.second->timeout_check(now,imp_->timeout_);
            }
            check();
        });
    }
}


