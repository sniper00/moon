#include "worker.h"
#include "common/time.hpp"
#include "common/string.hpp"
#include "common/hash.hpp"
#include "message.hpp"
#include "common/log.hpp"
#include "server.h"
#include "service.hpp"

namespace moon
{
    worker::worker(server* srv, router* r, uint32_t id)
        : workerid_(id)
        , router_(r)
        , server_(srv)
        , io_ctx_(1)
        , work_(asio::make_work_guard(io_ctx_))
    {
    }

    worker::~worker()
    {
        wait();
    }

    std::string worker::info()
    {
        auto response = moon::format(
            R"({"cpu":%lld,"socket_num":%zu,"mqsize":%d, "timer":%zu})",
            cpu_cost_,
            socket_->socket_num(),
            mqsize_.load(),
            timer_.size()
        );
        cpu_cost_ = 0;
        return response;
    }

    void worker::run()
    {
        //register commands

        commands_.try_emplace("services", [this](const std::vector<std::string>& params) {
            (void)params;
            std::string content;
            content.append("[");
            for (auto& it : services_)
            {
                content.append(moon::format(
                    R"({"name":"%s","serviceid":"%X"},)",
                    it.second->name().data(),
                    it.second->id()));
            }
            content.back() = ']';
            return content;
            });

        socket_ = std::make_unique<moon::socket>(router_, this, io_ctx_);

        thread_ = std::thread([this]() {
            state_.store(state::ready, std::memory_order_release);
            CONSOLE_INFO(router_->logger(), "WORKER-%u START", workerid_);
            io_ctx_.run();
            services_.clear();
            CONSOLE_INFO(router_->logger(), "WORKER-%u STOP", workerid_);
            });

        while (state_.load(std::memory_order_acquire) == state::init)
        {
            std::this_thread::yield();
        }
    }

    void worker::stop()
    {
        asio::post(io_ctx_, [this] {
            if (auto s = state_.load(std::memory_order_acquire); s == state::stopping || s == state::stopped)
            {
                return;
            }

            if (services_.empty())
            {
                state_.store(state::stopped, std::memory_order_release);
                return;
            }
            state_.store(state::stopping, std::memory_order_release);

            message msg;
            msg.set_type(PTYPE_SHUTDOWN);
            for (auto& it : services_)
            {
                it.second->dispatch(&msg);
            }
         });
    }

    void worker::wait()
    {
        io_ctx_.stop();
        if (thread_.joinable())
        {
            thread_.join();
        }
    }

    bool worker::stoped() const
    {
        return (state_.load(std::memory_order_acquire) == state::stopped);
    }

    uint32_t worker::uuid()
    {
        auto res = uuid_++;
        res %= max_uuid;
        ++res;
        res |= id() << WORKER_ID_SHIFT;
        return res;
    }

    void worker::add_service(std::string service_type
        , std::string config
        , bool unique
        , uint32_t creatorid
        , int32_t sessionid)
    {
        asio::post(io_ctx_, [this, service_type = std::move(service_type), config = std::move(config), unique, creatorid, sessionid](){
            do
            {
                if (state_.load(std::memory_order_acquire) != state::ready)
                {
                    break;
                }

                size_t counter = 0;
                uint32_t serviceid = 0;
                do
                {
                    if (counter >= worker::max_uuid)
                    {
                        serviceid = 0;
                        CONSOLE_ERROR(router_->logger()
                            , "new service failed: can not get more service id. worker[%d] service num[%u].", id(), services_.size());
                        break;
                    }
                    serviceid = uuid();
                    ++counter;
                } while (services_.find(serviceid) != services_.end());

                if (serviceid == 0)
                {
                    break;
                }

                auto s = router_->make_service(service_type);
                MOON_ASSERT(s,
                    moon::format("new service failed:service type[%s] was not registered", service_type.data()).data());
                s->set_id(serviceid);
                s->logger(router_->logger());
                s->set_unique(unique);
                s->set_server_context(server_, router_, this);

                if (!s->init(config))
                {
                    break;
                }

                s->ok(true);

                auto res = services_.emplace(serviceid, std::move(s));
                if (!res.second)
                {
                    break;
                }

                count_.fetch_add(1, std::memory_order_release);

                if (0 != sessionid)
                {
                    router_->response(creatorid, std::string_view{}, std::to_string(serviceid), sessionid);
                }
                return;
            } while (false);

            if (services_.empty())
            {
                shared(true);
            }

            if (0 != sessionid)
            {
                router_->response(creatorid, std::string_view{}, "0"sv, sessionid);
            }
        });
    }

    void worker::remove_service(uint32_t serviceid, uint32_t sender, uint32_t sessionid)
    {
        asio::post(io_ctx_, [this, serviceid, sender, sessionid]() {
            if (auto s = find_service(serviceid); nullptr != s)
            {
                count_.fetch_sub(1, std::memory_order_release);

                auto content = moon::format(R"({"name":"%s","serviceid":%08X,"errmsg":"service destroy"})", s->name().data(), s->id());
                router_->response(sender, "service destroy"sv, content, sessionid);
                services_.erase(serviceid);
                if (services_.empty()) shared(true);

                if (server_->get_state() == state::ready)
                {
                    std::string_view header{ "_service_exit" };
                    auto buf = message::create_buffer();
                    buf->write_back(content.data(), content.size());
                    router_->broadcast(serviceid, buf, header, PTYPE_SYSTEM);
                }
            }
            else
            {
                router_->response(sender, "worker::remove_service "sv, moon::format("service [%08X] not found", serviceid), sessionid, PTYPE_ERROR);
            }

            if (services_.size() == 0 && (state_.load() == state::stopping))
            {
                state_.store(state::stopped, std::memory_order_release);
            }
            });
    }

    asio::io_context& worker::io_context()
    {
        return io_ctx_;
    }

    void worker::send(message_ptr_t&& msg)
    {
        ++mqsize_;
        if (mq_.push_back(std::move(msg)) == 1)
        {
            asio::post(io_ctx_, [this]() {
                if (mq_.size() == 0)
                {
                    return;
                }

                service* ser = nullptr;
                mq_.swap(swapmq_);
                for (auto& msg : swapmq_)
                {
                    handle_one(ser, std::move(msg));
                    --mqsize_;
                }
                swapmq_.clear();
                });
        }
    }

    uint32_t worker::id() const
    {
        return workerid_;
    }

    service* worker::find_service(uint32_t serviceid) const
    {
        auto iter = services_.find(serviceid);
        if (services_.end() != iter)
        {
            return iter->second.get();
        }
        return nullptr;
    }

    void worker::on_timer(timer_t timerid, uint32_t serviceid, bool last)
    {
        if (auto s = find_service(serviceid); nullptr != s)
        {
            int64_t start_time = moon::time::microsecond();

            message msg;
            msg.set_type(PTYPE_TIMER);
            msg.set_sender(timerid);
            msg.set_receiver(last ? 1 : 0);
            s->dispatch(&msg);
            int64_t cost_time = moon::time::microsecond() - start_time;
            s->add_cpu_cost(cost_time);
            cpu_cost_ += cost_time;
            if (cost_time > 100000)
            {
                CONSOLE_WARN(router_->logger(),
                    "worker %u on timer cost %" PRId64 "us, owner %08X timerid %u", id(), cost_time, serviceid, timerid);
            }
        }
        else
        {
            timer_.remove(timerid);
        }
    }

    void worker::runcmd(uint32_t sender, const std::string& cmd, int32_t sessionid)
    {
        asio::post(io_ctx_, [this, sender, cmd, sessionid] {
            auto params = moon::split<std::string>(cmd, ".");

            switch (moon::chash_string(params[0]))
            {
            case "worker"_csh:
            {
                if (auto iter = commands_.find(params[2]); iter != commands_.end())
                {
                    router_->response(sender, std::string_view{}, iter->second(params), sessionid);
                }
                break;
            }
            }
            });
    }

    uint32_t worker::make_prefab(const moon::buffer_ptr_t& buf)
    {
        auto iter = prefabs_.emplace(uuid(), buf);
        if (iter.second)
        {
            return iter.first->first;
        }
        return 0;
    }

    void worker::send_prefab(uint32_t sender
        , uint32_t receiver
        , uint32_t prefabid
        , std::string_view header
        , int32_t sessionid
        , uint8_t type) const
    {
        if (auto iter = prefabs_.find(prefabid); iter != prefabs_.end())
        {
            router_->send(sender, receiver, iter->second, header, sessionid, type);
            return;
        }
        CONSOLE_DEBUG(server_->logger(), "send_prefab failed, can not find prepared data. prefabid %u", prefabid);
    }

    timer_t worker::repeat(int64_t interval, int32_t times, uint32_t serviceid)
    {
        return timer_.repeat(server_->now(), interval, times, serviceid, this);
    }

    void worker::remove_timer(timer_t id)
    {
        timer_.remove(id);
    }

    void worker::shared(bool v)
    {
        shared_ = v;
    }

    bool worker::shared() const
    {
        return shared_.load();
    }

    void worker::update()
    {
        //update_state is true
        if (update_state_.test_and_set(std::memory_order_acquire))
        {
            return;
        }

        asio::post(io_ctx_, [this] {
            timer_.update(server_->now());

            if (!prefabs_.empty())
            {
                prefabs_.clear();
            }

            update_state_.clear(std::memory_order_release);
         });
    }

    void worker::handle_one(service*& s, message_ptr_t&& msg)
    {
        uint32_t sender = msg->sender();
        uint32_t receiver = msg->receiver();
        if (msg->broadcast())
        {
            for (auto& it : services_)
            {
                if (!it.second->unique() && msg->type() == PTYPE_SYSTEM)
                {
                    continue;
                }

                if (it.second->ok() && it.second->id() != sender)
                {
                    it.second->handle_message(std::move(msg));
                }
            }
            return;
        }

        if (nullptr == s || s->id() != receiver)
        {
            s = find_service(receiver);
            if (nullptr == s || !s->ok())
            {
                if (sender != 0)
                {
                    std::string hexdata = moon::hex_string({ msg->data(),msg->size() });
                    std::string str = moon::format("[%08X] attempt send to dead service [%08X]: %s."
                        , sender
                        , receiver
                        , hexdata.data());

                    msg->set_sessionid(-msg->sessionid());
                    router_->response(sender, "worker::handle_one "sv, str, msg->sessionid(), PTYPE_ERROR);
                }
                return;
            }
        }

        int64_t start_time = moon::time::microsecond();
        s->handle_message(std::move(msg));
        int64_t cost_time = moon::time::microsecond() - start_time;
        s->add_cpu_cost(cost_time);
        cpu_cost_ += cost_time;
        if (cost_time > 100000)
        {
            CONSOLE_WARN(router_->logger(),
                "worker %u handle one message cost %" PRId64 "us, from %08X to %08X", id(), cost_time, sender, receiver);
        }
        timer_.update(server_->now());
    }
}
