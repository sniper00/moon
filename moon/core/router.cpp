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

    uint32_t router::new_service(const std::string & service_type, const string_view_t& config, bool unique, int32_t workerid, uint32_t creatorid, int32_t responseid)
    {
        auto iter = regservices_.find(service_type);
        MOON_DCHECK(iter != regservices_.end(), moon::format("new service failed:service type[%s] was not registered", service_type.data()).data());
        worker* wk;
        if (workerid_valid(workerid))
        {
            wk = workers_[workerid - 1].get();
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
                return 0;
            }
            serviceid = wk->make_serviceid();
            ++counter;
        } while (!try_add_serviceid(serviceid));

        auto s = iter->second();
        s->set_id(serviceid);
        s->set_unique(unique);
        s->set_server_context(server_, this, wk);
        if (s->init(config))
        {
            if (!unique || unique_services_.set(s->name(), s->id()))
            {
                wk->add_service(std::move(s), creatorid, responseid);
                if (workerid != 0)
                {
                    wk->shared(false);
                }
                return serviceid;
            }
        }
        on_service_remove(serviceid);
        CONSOLE_ERROR(logger(), "init service failed with config: %s", std::string{ config.data(),config.size() }.data());
        return 0;
    }

    void router::remove_service(uint32_t serviceid, uint32_t sender, int32_t responseid, bool crashed)
    {
        auto workerid = worker_id(serviceid);
        if (workerid_valid(workerid))
        {
            workers_[workerid - 1]->remove_service(serviceid, sender, responseid, crashed);
        }
        else
        {
            auto content = moon::format("worker %d not found.", workerid);
            make_response(sender, "router::remove_service "sv, content, responseid, PTYPE_ERROR);
        }
    }

    void  router::runcmd(uint32_t sender, const std::string& cmd, int32_t responseid)
    {
        auto params = moon::split<std::string>(cmd, ".");
        if (params.size() < 3)
        {
            make_response(sender, "router::runcmd "sv, moon::format("param too few: %s", cmd.data()), responseid, PTYPE_ERROR);
            return;
        }

        switch (moon::chash_string(params[0]))
        {
        case "worker"_csh:
        {
            int32_t workerid = moon::string_convert<int32_t>(params[1]);
            if (workerid_valid(workerid))
            {
                workers_[workerid - 1]->runcmd(sender, cmd, responseid);
                return;
            }
            break;
        }
        }

        auto content = moon::format("invalid cmd: %s.", cmd.data());
        make_response(sender, "router::runcmd "sv, content, responseid, PTYPE_ERROR);
    }

    void router::send_message(message_ptr_t&& msg) const
    {
        MOON_CHECK(msg->type() != PTYPE_UNKNOWN, "invalid message type.");
        MOON_CHECK(msg->receiver() != 0, "message receiver serviceid is 0.");
        int32_t id = worker_id(msg->receiver());
        MOON_CHECK(workerid_valid(id), "invalid message receiver serviceid.");
        workers_[id - 1]->send(std::forward<message_ptr_t>(msg));
    }

    void router::send(uint32_t sender, uint32_t receiver, const buffer_ptr_t & data, const string_view_t& header, int32_t responseid, uint8_t type) const
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

    void router::broadcast(uint32_t sender, const buffer_ptr_t& buf, const string_view_t& header, uint8_t type)
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
        MOON_DCHECK(ret.second, moon::format("already registed service type[%s].", type.data()).data());
        return ret.second;
    }

    std::shared_ptr<std::string> router::get_env(const std::string & name) const
    {
        std::string v;
        if (env_.try_get_value(name, v))
        {
            return std::make_shared<std::string>(std::move(v));
        }
        return nullptr;
    }

    void router::set_env(const string_view_t & name, const string_view_t & value)
    {
        env_.set(std::string(name.data(), name.size()), std::string(value.data(), value.size()));
    }

    uint32_t router::get_unique_service(const string_view_t & name) const
    {
        if (name.empty())
        {
            return 0;
        }

        uint32_t id = 0;
        unique_services_.try_get_value(std::string(name.data(), name.size()), id);
        return id;
    }

    void router::set_unique_service(const string_view_t & name, uint32_t v)
    {
        if (name.empty())
        {
            return;
        }
        unique_services_.set(std::string(name.data(), name.size()), v);
    }

    log * router::logger() const
    {
        return logger_;
    }

    void router::make_response(uint32_t sender, const string_view_t&header, const string_view_t& content, int32_t responseid, uint8_t mtype) const
    {
        if (sender == 0 || responseid == 0)
        {
            if (mtype == PTYPE_ERROR && !content.empty())
            {
                CONSOLE_DEBUG(logger(), "server::make_response %s:%s", std::string(header.data(), header.size()).data(), std::string(content.data(), content.size()).data());
            }
            return;
        }

        auto rmsg = message::create(content.size());
        rmsg->set_header(header);
        rmsg->set_receiver(sender);
        rmsg->set_type(mtype);
        rmsg->set_responseid(responseid);
        rmsg->write_data(content);
        send_message(std::move(rmsg));
    }

    worker* router::next_worker()
    {
        int32_t  n = next_workerid_.fetch_add(1);
        std::vector<int32_t> free_worker;
        for (auto& w : workers_)
        {
            if (w->shared())
            {
                free_worker.push_back(w->id()-1);
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
        return workers_[workerid - 1]->io_context();
    }

    void router::set_server(server * sv)
    {
        server_ = sv;
    }
}
