/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#include "server.h"
#include "common/string.hpp"
#include "common/concurrent_map.hpp"
#include "common/time.hpp"
#include "common/rwlock.hpp"
#include "common/log.hpp"
#include "worker.h"
#include "message.hpp"
#include "service.h"

namespace moon
{
    struct server::server_imp
    {
		using worker_ptr_t = std::unique_ptr<worker>;
        using env_t = concurrent_map<std::string, std::string, rwlock>;
        using unique_service_db_t = concurrent_map<std::string, uint32_t, rwlock>;

        server_imp()
            :state_(state::init)
            , workernum_(0)
            , next_workerid_(0)
        {
        }

        uint8_t next_worker_id()
        {
            uint32_t  id = next_workerid_.fetch_add(1);
            id %= workernum_;
            return static_cast<uint8_t>(id);
        }

        worker_ptr_t& next_worker()
        {
            uint8_t  id = 0;
            for (uint8_t i = 0; i < workernum_; i++)
            {
                id = next_worker_id();
                auto& w = workers_[id];
                if (!w->shared())
                    continue;
                return w;
            }

            return workers_[id];
        }

        bool has_serviceid(uint32_t serviceid) const
        {
            SHARED_LOCK_GURAD(serviceids_lck_);
            return (serviceids_.find(serviceid) != serviceids_.end());
        }

        bool try_add_serviceid(uint32_t serviceid)
        {
            UNIQUE_LOCK_GURAD(serviceids_lck_);
            if (serviceids_.find(serviceid) == serviceids_.end())
            {
                return serviceids_.emplace(serviceid).second;
            }
            return false;
        }

        void on_service_remove(uint32_t serviceid)
        {
            UNIQUE_LOCK_GURAD(serviceids_lck_);
            MOON_CHECK(serviceids_.erase(serviceid)==1,"erase failed!");
        }

        size_t servicenum() const
        {
            SHARED_LOCK_GURAD(serviceids_lck_);
            return serviceids_.size();
        }

        void wait()
        {
            for (auto iter = workers_.rbegin(); iter != workers_.rend(); ++iter)
            {
                (*iter)->wait();
            }

            CONSOLE_INFO((&default_log_), "STOP");
            default_log_.wait();
			state_.store(state::exited);
        }

		std::atomic<state> state_;
        uint8_t workernum_;
        std::atomic<uint32_t> next_workerid_;
        std::vector<worker_ptr_t> workers_;
        std::unordered_map<std::string, register_func > regservices_;
        mutable rwlock serviceids_lck_;
        std::unordered_set<uint32_t> serviceids_;
        env_t env_;
        unique_service_db_t unique_services_;
        log default_log_;
    };

    server::server()
        :imp_(new server_imp)
    {

    }

    server::~server()
    {
        stop();
        imp_->wait();
        SAFE_DELETE(imp_);
    }

    void server::init(uint8_t worker_num, const std::string& logpath)
    {
        worker_num == 0 ? 1 : worker_num;
        imp_->workernum_ = worker_num;

		logger()->init(logpath);
		CONSOLE_INFO(logger(), "INIT with %d workers.", imp_->workernum_);

        for (uint8_t i = 0; i != worker_num; i++)
        {
			auto& w = imp_->workers_.emplace_back(std::make_unique<worker>());
            w->workerid(i+1);
            w->set_server(this);
            w->on_service_remove = std::bind(&server_imp::on_service_remove, imp_, std::placeholders::_1);
        }

        for (auto& w : imp_->workers_)
        {
            w->run();
        }

		imp_->state_.store(state::ready);
    }

    uint8_t server::workernum() const
    {
        return static_cast<uint8_t>(imp_->workers_.size());
    }

    size_t server::servicenum() const
    {
        return imp_->servicenum();
    }

    void server::run()
    {
        if (0 == imp_->workernum_)
        {
            printf("should run server::init first!\r\n");
            return;
        }

        for (auto& w : imp_->workers_)
        {
            w->start();
        }

        int64_t prew_tick = time::millsecond();
        int64_t prev_sleep_time = 0;
        while (true)
        {
            auto now = time::millsecond();
            auto diff = (now - prew_tick);
            prew_tick = now;

            int stoped_worker_num = 0;

            for (auto& w : imp_->workers_)
            {
                if (w->stoped())
                {
                    stoped_worker_num++;
                }
                w->update();
            }

            if (stoped_worker_num == imp_->workernum_)
            {
                break;
            }

            if (diff <= EVENT_UPDATE_INTERVAL + prev_sleep_time)
            {
                prev_sleep_time = EVENT_UPDATE_INTERVAL + prev_sleep_time - diff;
                thread_sleep(prev_sleep_time);
            }
            else
            {
                prev_sleep_time = 0;
            }
        }

        imp_->wait();
    }

    void server::stop()
    {
		if (imp_->state_.load()!=state::ready)
		{
			return;
		}

		for (int i = 0; i < 100; i++)
		{
			for (auto iter = imp_->workers_.rbegin(); iter != imp_->workers_.rend(); ++iter)
			{
				(*iter)->stop();
			}
		}
    }

    uint32_t server::new_service(const std::string & service_type, bool unique, bool shared, int workerid, const string_view_t& config)
    {
		if (imp_->state_.load(std::memory_order_acquire) != state::ready)
		{
			return 0;
		}

        auto iter = imp_->regservices_.find(service_type);
        if (iter == imp_->regservices_.end())
        {
            CONSOLE_ERROR(logger(),"new service failed:service type[%s] was not registered", service_type.data());
            return 0;
        }

        auto s = iter->second();

        worker* wk;
        if (workerid>0 && workerid <= static_cast<int>(imp_->workernum_))
        {
            wk = imp_->workers_[workerid - 1].get();
        }
        else
        {
            wk = imp_->next_worker().get();
        }

        size_t counter = 0;
        uint32_t serviceid = 0;
        do
        {
            if (counter>= worker::MAX_SERVICE_NUM)
            {
                CONSOLE_ERROR(logger(),"new service failed: can not get more service id.worker[%d] servicenum[%u].", wk->workerid(), wk->servicenum());
                return 0;
            }
            serviceid = wk->make_serviceid();
            ++counter;
        } while (!imp_->try_add_serviceid(serviceid));

        wk->shared(shared);
        s->set_id(serviceid);
        s->set_worker(wk);
        s->set_unique(unique);

        if (s->init(config))
        {
            if (!unique || imp_->unique_services_.set(s->name(), s->id()))
            {
                wk->add_service(s);
                return serviceid;
            }
        }
        imp_->on_service_remove(serviceid);
        CONSOLE_ERROR(logger(), "init service failed with config: %s", std::string{ config.data(),config.size() }.data());
        return 0;
    }

    void server::runcmd(uint32_t sender, const buffer_ptr_t& buf, const string_view_t& header, int32_t responseid)
    {
        (void)buf;

		if (imp_->state_.load(std::memory_order_acquire) != state::ready)
		{
			return;
		}

        responseid = -responseid;

        auto cmds = moon::split<string_view_t>(header, ".");
        if (cmds.size() == 0)
        {
            make_response(sender,"error"sv,"server: call cmd empty"sv, responseid, PTYPE_ERROR);
            return;
        }

        if (cmds.front() == "rmservice")
        {
            if (cmds.size() < 2)
            {
                auto content = moon::format("server:  call rmservice param error %s.", header.data());
                make_response(sender, "error",content, responseid, PTYPE_ERROR);
                return;
            }

            uint32_t serviceid = moon::string_convert<uint32_t>(cmds[1]);
            if (0 == serviceid)
            {
                make_response(sender, "error"sv, "remove service id = 0"sv, responseid, PTYPE_ERROR);
                return;
            }

            auto workerid =worker_id(serviceid);
            if (workerid>0 && workerid <= imp_->workernum_)
            {
                imp_->workers_[workerid-1]->remove_service(serviceid, sender, responseid);
            }
            else
            {
                auto content = moon::format("rmservice worker %d not found.",workerid);
                make_response(sender, "error"sv, content, responseid, PTYPE_ERROR);
            }
        }
        else if (cmds.front() == "workertime")
        {
            if (cmds.size() < 2)
            {
                auto content = moon::format("server: call workerrate param error %s", header.data());
                make_response(sender, "error"sv, content, responseid, PTYPE_ERROR);
                return;
            }

            uint32_t workerid = moon::string_convert<uint32_t>(cmds[1]);
            if (workerid>0 && workerid  <= imp_->workernum_)
            {
                imp_->workers_[workerid-1]->worker_time(sender, responseid);
            }
            else
            {
                auto content = moon::format("service worker %d not found.", workerid);
                make_response(sender, "error"sv, content, responseid, PTYPE_ERROR);
            }
        }
        else
        {
            auto content = moon::format("server: call invalid  cmd %s.", std::string{ cmds.front().data(),cmds.front().size() });
            make_response(sender, "error"sv, content, responseid, PTYPE_ERROR);
        }
        return;
    }

    void server::send_message(const message_ptr_t & msg) const
    {
		if (imp_->state_.load(std::memory_order_acquire) != state::ready)
		{
			return;
		}
        MOON_DCHECK(msg->type() != PTYPE_UNKNOWN, "invalid message type.");
        MOON_DCHECK(msg->receiver() != 0, "message receiver serviceid is 0.");
        uint8_t wkid =worker_id(msg->receiver());
        MOON_DCHECK(wkid > 0 && wkid <= imp_->workernum_, "invalid message receiver serviceid.");
        imp_->workers_[wkid -1]->send(msg);
    }

    void server::send(uint32_t sender, uint32_t receiver, const buffer_ptr_t & data, const string_view_t& header, int32_t responseid, uint8_t type) const
    {
        responseid = -responseid;
        message_ptr_t msg = std::make_shared<message>(data);
        msg->set_sender(sender);
        msg->set_receiver(receiver);
        if (header.size() != 0)
        {
            msg->set_header(header);
        }
        msg->set_type(type);
        msg->set_responseid(responseid);
        send_message(msg);
    }

    void server::broadcast(uint32_t sender, const message_ptr_t & msg)
    {
		if (imp_->state_.load(std::memory_order_acquire) != state::ready)
		{
			return;
		}

        MOON_DCHECK(msg->type() != PTYPE_UNKNOWN,"invalid message type.");
        msg->set_broadcast(true);
        msg->set_sender(sender);
        for (auto& w : imp_->workers_)
        {
            w->send(msg);
        }
    }

    bool server::register_service(const std::string & type, register_func f)
    {
        auto ret = imp_->regservices_.emplace(type, f);
        MOON_DCHECK(ret.second, moon::format("already registed message type[%s].",type.data()).data());
        return ret.second;
    }

    std::shared_ptr<std::string> server::get_env(const std::string & name) const
    {
        std::string v;
		if (imp_->env_.try_get_value(name, v))
		{
			return std::make_shared<std::string>(std::move(v));
		}
        return nullptr;
    }

    void server::set_env(const string_view_t & name, const string_view_t & value)
    {
        imp_->env_.set(std::string(name.data(), name.size()), std::string(value.data(), value.size()));
    }

    uint32_t server::get_unique_service(const string_view_t & name) const
    {
        if (name.empty())
        {
            return 0;
        }

        uint32_t id = 0;
        imp_->unique_services_.try_get_value(std::string(name.data(), name.size()), id);
        return id;
    }

    void server::set_unique_service(const string_view_t & name, uint32_t v)
    {
        if (name.empty())
        {
            return;
        }
        imp_->unique_services_.set(std::string(name.data(), name.size()), v);
    }

    log * server::logger() const
    {
        return &imp_->default_log_;
    }

    void server::make_response(uint32_t sender, const string_view_t&header,const string_view_t& content, int32_t resp, uint8_t mtype) const
    {
        if (mtype== PTYPE_ERROR && !content.empty())
        {
            CONSOLE_DEBUG(logger(),"server::make_response %s:%s",std::string(header.data(),header.size()).data(), std::string(content.data(),content.size()).data());
        }

        if (sender == 0 || resp == 0)
        {
            return;
        }

        auto rmsg = message::create(content.size());
        rmsg->set_header(header);
        rmsg->set_receiver(sender);
        rmsg->set_type(mtype);
        rmsg->set_responseid(-resp);
        rmsg->write_data(content);
        send_message(rmsg);
    }
}


