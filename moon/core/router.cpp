#include "router.h"
#include "common/string.hpp"
#include "common/hash.hpp"
#include "common/time.hpp"
#include "worker.h"
#include "message.hpp"
#include "service.h"
#include "server.h"

namespace moon
{
    router::router(std::vector<std::unique_ptr<worker>>& workers, log* logger)
        :next_workerid_(0)
        , workers_(workers)
        , logger_(logger)
        , server_(nullptr)
    {
    }

    size_t router::servicenum() const
    {
        SHARED_LOCK_GURAD(serviceids_lck_);
        return serviceids_.size();
    }

    size_t router::workernum() const
    {
        return workers_.size();
    }

    bool router::new_service(const std::string & service_type,const std::string& config, bool unique, int32_t workerid, uint32_t creatorid, int32_t responseid)
    {
        auto iter = regservices_.find(service_type);
        MOON_ASSERT(iter != regservices_.end(), moon::format("new service failed:service type[%s] was not registered", service_type.data()).data());
        worker* wk;
        if (workerid_valid(workerid))
        {
            wk = get_worker(workerid);
        }
        else
        {
            wk = next_worker();
        }

        size_t counter = 0;
        uint32_t serviceid = 0;
        do
        {
            if (counter >= worker::MAX_SERVICE_NUM)
            {
                CONSOLE_ERROR(logger(), "new service failed: can not get more service id.worker[%d] servicenum[%u].", wk->id(), wk->size());
                return false;
            }
            serviceid = wk->make_serviceid();
            ++counter;
        } while (!try_add_serviceid(serviceid));

        if (workerid != 0)
        {
            wk->shared(false);
        }

        auto s = iter->second();
        s->set_id(serviceid);
        s->set_unique(unique);
        s->set_server_context(server_, this, wk);
        wk->add_service(std::move(s), config, creatorid, responseid);
        return true;
    }

    void router::remove_service(uint32_t serviceid, uint32_t sender, int32_t responseid, bool crashed)
    {
        auto workerid = worker_id(serviceid);
        if (workerid_valid(workerid))
        {
            get_worker(workerid)->remove_service(serviceid, sender, responseid, crashed);
        }
        else
        {
            auto content = moon::format("worker %d not found.", workerid);
            response(sender, "router::remove_service "sv, content, responseid, PTYPE_ERROR);
        }
    }

    void  router::runcmd(uint32_t sender, const std::string& cmd, int32_t responseid)
    {
        auto params = moon::split<std::string>(cmd, ".");
        if (params.size() < 3)
        {
            response(sender, "router::runcmd "sv, moon::format("param too few: %s", cmd.data()), responseid, PTYPE_ERROR);
            return;
        }

        switch (moon::chash_string(params[0]))
        {
        case "worker"_csh:
        {
            int32_t workerid = moon::string_convert<int32_t>(params[1]);
            if (workerid_valid(workerid))
            {
                get_worker(workerid)->runcmd(sender, cmd, responseid);
                return;
            }
            break;
        }
        }

        auto content = moon::format("invalid cmd: %s.", cmd.data());
        response(sender, "router::runcmd "sv, content, responseid, PTYPE_ERROR);
    }

    void router::send_message(message_ptr_t&& msg) const
    {
        MOON_CHECK(msg->type() != PTYPE_UNKNOWN, "invalid message type.");
        MOON_CHECK(msg->receiver() != 0, "message receiver serviceid is 0.");
        int32_t id = worker_id(msg->receiver());
        MOON_CHECK(workerid_valid(id),moon::format("invalid message receiver serviceid %u",msg->receiver()).data());
        get_worker(id)->send(std::forward<message_ptr_t>(msg));
    }

    void router::send(uint32_t sender, uint32_t receiver, const buffer_ptr_t & data, string_view_t header, int32_t responseid, uint8_t type) const
    {
        responseid = -responseid;
        message_ptr_t msg = message::create(data);
        msg->set_sender(sender);
        msg->set_receiver(receiver);
        if (header.size() != 0)
        {
            msg->set_header(header);
        }
        msg->set_type(type);
        msg->set_responseid(responseid);
        send_message(std::move(msg));
    }

    void router::broadcast(uint32_t sender, const buffer_ptr_t& buf, string_view_t header, uint8_t type)
    {
        for (auto& w : workers_)
        {
            auto m = message::create(buf);
            m->set_broadcast(true);
            m->set_header(header);
            m->set_sender(sender);
            m->set_type(type);
            w->send(std::move(m));
        }
    }

    bool router::register_service(const std::string & type, register_func f)
    {
        auto ret = regservices_.emplace(type, f);
        MOON_ASSERT(ret.second, moon::format("already registed service type[%s].", type.data()).data());
        return ret.second;
    }

    std::string router::get_env(const std::string & name) const
    {
        std::string v;
        if (env_.try_get_value(name, v))
        {
            return (std::move(v));
        }
        return std::string{};
    }

    void router::set_env(std::string name,  std::string value)
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
        return unique_services_.set(std::move(name), v);
    }

    size_t router::unique_service_size() const
    {
        return unique_services_.size();
    }

    log * router::logger() const
    {
        return logger_;
    }

    void router::response(uint32_t to, string_view_t header, string_view_t content, int32_t responseid, uint8_t mtype) const
    {
        if (to == 0 || responseid == 0)
        {
            if (mtype == PTYPE_ERROR && !content.empty())
            {
                CONSOLE_DEBUG(logger(), "server::make_response %s:%s", std::string(header.data(), header.size()).data(), std::string(content.data(), content.size()).data());
            }
            return;
        }

        auto rmsg = message::create(content.size());
        rmsg->set_receiver(to);
        rmsg->set_header(header);
        rmsg->set_type(mtype);
        rmsg->set_responseid(responseid);
        rmsg->write_data(content);
        send_message(std::move(rmsg));
    }

    worker* router::next_worker()
    {
        uint32_t  n = next_workerid_.fetch_add(1);
        std::vector<uint32_t> free_worker;
        for (auto& w : workers_)
        {
            if (w->shared())
            {
                free_worker.push_back(w->id()-1U);
            }
        }
        if (!free_worker.empty())
        {
            auto wkid = free_worker[n%free_worker.size()];
            return workers_[wkid].get();
        }
        n %= workers_.size();
        return workers_[n].get();
    }

    bool router::has_serviceid(uint32_t serviceid) const
    {
        SHARED_LOCK_GURAD(serviceids_lck_);
        return (serviceids_.find(serviceid) != serviceids_.end());
    }

    bool router::try_add_serviceid(uint32_t serviceid)
    {
        UNIQUE_LOCK_GURAD(serviceids_lck_);
        if (serviceids_.find(serviceid) == serviceids_.end())
        {
            return serviceids_.emplace(serviceid).second;
        }
        return false;
    }

    void router::on_service_remove(uint32_t serviceid)
    {
        UNIQUE_LOCK_GURAD(serviceids_lck_);
        size_t count = serviceids_.erase(serviceid);
        MOON_CHECK(count == 1, "erase failed!");
    }

    asio::io_context & router::get_io_context(uint32_t serviceid)
    {
        int32_t workerid = worker_id(serviceid);
        return get_worker(workerid)->io_context();
    }

    void router::set_server(server * sv)
    {
        server_ = sv;
    }
}
