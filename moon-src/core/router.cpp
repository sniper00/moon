#include "router.h"
#include "common/string.hpp"
#include "common/hash.hpp"
#include "common/time.hpp"
#include "worker.h"
#include "message.hpp"
#include "server.h"
#include "service.hpp"

namespace moon
{
    void router::init(server* svr)
    {
        server_ = svr;
        logger_ = svr->logger();
    }

    void router::new_service(std::string service_type
        , std::string config
        , bool unique
        , int32_t workerid
        , uint32_t creatorid
        , int32_t sessionid)
    {
        worker* w = server_->get_worker(workerid);
        if (nullptr != w)
        {
            w->shared(false);
        }
        else
        {
            w = server_->next_worker();
        }
        w->add_service(std::move(service_type), std::move(config), unique, creatorid, sessionid);
    }

    void router::remove_service(uint32_t serviceid, uint32_t sender, int32_t sessionid)
    {
        auto workerid = worker_id(serviceid);
        worker* w = server_->get_worker(workerid);
        if (nullptr != w)
        {
            w->remove_service(serviceid, sender, sessionid);
        }
        else
        {
            auto content = moon::format("worker %d not found.", workerid);
            response(sender, "router::remove_service "sv, content, sessionid, PTYPE_ERROR);
        }
    }

    void  router::runcmd(uint32_t sender, const std::string& cmd, int32_t sessionid)
    {
        auto params = moon::split<std::string>(cmd, ".");
        if (params.size() < 3)
        {
            response(sender, "router::runcmd "sv
                , moon::format("param too few: %s", cmd.data()), sessionid, PTYPE_ERROR);
            return;
        }

        switch (moon::chash_string(params[0]))
        {
        case "worker"_csh:
        {
            int32_t workerid = moon::string_convert<int32_t>(params[1]);
            worker* w = server_->get_worker(workerid);
            if (nullptr != w)
            {
                w->runcmd(sender, cmd, sessionid);
                return;
            }
            break;
        }
        }

        auto content = moon::format("invalid cmd: %s.", cmd.data());
        response(sender, "router::runcmd "sv, content, sessionid, PTYPE_ERROR);
    }

    void router::send_message(message_ptr_t&& m) const
    {
        MOON_CHECK(m->type() != PTYPE_UNKNOWN, "invalid message type.");
        MOON_CHECK(m->receiver() != 0, "message receiver serviceid is 0.");
        int32_t id = worker_id(m->receiver());
        worker* w = server_->get_worker(id);
        MOON_CHECK(nullptr!= w
            , moon::format("invalid message receiver serviceid %X", m->receiver()).data());
        w->send(std::forward<message_ptr_t>(m));
    }

    void router::send(uint32_t sender
        , uint32_t receiver
        , buffer_ptr_t data
        , std::string_view header
        , int32_t sessionid
        , uint8_t type) const
    {
        sessionid = -sessionid;
        message_ptr_t m = message::create(std::move(data));
        m->set_sender(sender);
        m->set_receiver(receiver);
        if (header.size() != 0)
        {
            m->set_header(header);
        }
        m->set_type(type);
        m->set_sessionid(sessionid);
        send_message(std::move(m));
    }

    void router::broadcast(uint32_t sender, const buffer_ptr_t& buf, std::string_view header, uint8_t type)
    {
        for (auto& w : server_->get_workers())
        {
            auto m = message::create(buf);
            m->set_broadcast(true);
            m->set_header(header);
            m->set_sender(sender);
            m->set_type(type);
            w->send(std::move(m));
        }
    }

    bool router::register_service(const std::string& type, register_func f)
    {
        auto ret = regservices_.emplace(type, f);
        MOON_ASSERT(ret.second
            , moon::format("already registed service type[%s].", type.data()).data());
        return ret.second;
    }

    service_ptr_t router::make_service(const std::string& type)
    {
        auto iter = regservices_.find(type);
        if (iter != regservices_.end())
        {
            return iter->second();
        }
        return nullptr;
    }

    std::string router::get_env(const std::string& name) const
    {
        std::string v;
        if (env_.try_get_value(name, v))
        {
            return v;
        }
        return std::string{};
    }

    void router::set_env(std::string name, std::string value)
    {
        env_.set(std::move(name), std::move(value));
    }

    uint32_t router::get_unique_service(const std::string& name) const
    {
        if (name.empty())
        {
            return 0;
        }
        uint32_t id = 0;
        unique_services_.try_get_value(name, id);
        return id;
    }

    bool router::set_unique_service(std::string name, uint32_t v)
    {
        if (name.empty())
        {
            return false;
        }
        return unique_services_.try_set(std::move(name), v);
    }

    size_t router::unique_service_size() const
    {
        return unique_services_.size();
    }

    log* router::logger() const
    {
        return logger_;
    }

    void router::response(uint32_t to
        , std::string_view header
        , std::string_view content
        , int32_t sessionid
        , uint8_t mtype) const
    {
        if (to == 0 || sessionid == 0)
        {
            if (server_->get_state() == state::ready && mtype == PTYPE_ERROR && !content.empty())
            {
                CONSOLE_DEBUG(logger()
                    , "router response %s:%s"
                    , std::string(header).data()
                    , std::string(content).data());
            }
            return;
        }

        auto m = message::create(content.size());
        m->set_receiver(to);
        m->set_header(header);
        m->set_type(mtype);
        m->set_sessionid(sessionid);
        m->write_data(content);
        send_message(std::move(m));
    }

    std::string router::worker_info(uint32_t workerid)
    {
        worker* w = server_->get_worker(workerid);
        if (nullptr == w)
        {
            return std::string{};
        }
        return w->info();
    }
}

