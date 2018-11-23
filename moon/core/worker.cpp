#include "worker.h"
#include "common/time.hpp"
#include "common/string.hpp"
#include "common/hash.hpp"
#include "service.h"
#include "message.hpp"
#include "common/log.hpp"
#include "router.h"

namespace moon
{
    worker::worker(router* r)
        : state_(state::init)
        , shared_(true)
        , workerid_(0)
        , serviceuid_(1)
        , work_time_(0)
        , router_(r)
        , io_ctx_(1)
        , work_(asio::make_work_guard(io_ctx_))
    {
    }

    worker::~worker()
    {
    }

    void worker::run()
    {
        register_commands();
        thread_ = std::thread([this]() {
            state_.store(state::ready, std::memory_order_release);
            CONSOLE_INFO(router_->logger(), "WORKER-%d START", workerid_);
            io_ctx_.run();
            CONSOLE_INFO(router_->logger(), "WORKER-%d STOP", workerid_);
        });
        while (state_.load(std::memory_order_acquire) != state::ready);
    }

    void worker::stop()
    {
        post([this] {
            if (auto s = state_.load(std::memory_order_acquire); s == state::stopping || s == state::exited)
            {
                return;
            }

            if (services_.empty())
            {
                state_.store(state::exited, std::memory_order_release);
                return;
            }
            state_.store(state::stopping, std::memory_order_release);
            for (auto& it : services_)
            {
                auto& s = it.second;
                s->exit();
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

    bool worker::stoped()
    {
        return (state_.load(std::memory_order_acquire) == state::exited);
    }

    uint32_t worker::make_serviceid()
    {
        auto uid = serviceuid_.fetch_add(1);
        uid %= MAX_SERVICE_NUM;
        uint32_t tmp = uid + 1;
        int32_t wkid = workerid();
        tmp |= static_cast<uint32_t>(wkid) << WORKER_ID_SHIFT;
        return tmp;
    }

    void worker::add_service(service_ptr_t&& s)
    {
        //The operator () of a lambda is const by default
        post([this, s=std::move(s)]() mutable {
            auto id = s->id();
            auto res = services_.try_emplace(id, std::move(s));
            MOON_CHECK(res.second, "serviceid repeated");
            res.first->second->ok(true);
            servicenum_.store(static_cast<uint32_t>(services_.size()));
            CONSOLE_INFO(router_->logger(), "[WORKER %d] new service [%s:%u]", workerid(), res.first->second->name().data(), res.first->second->id());
        });
    }

    void worker::remove_service(uint32_t id, uint32_t sender, uint32_t respid, bool crashed)
    {
        post([this, id, sender, respid, crashed]() {
            if (auto iter = services_.find(id); services_.end() != iter)
            {
                std::string response_content;
                auto& s = iter->second;
                s->destroy();
                if (services_.size() == 0)
                {
                    shared(true);
                }
                response_content = moon::format(R"({"name":"%s","serviceid":%u})", s->name().data(), s->id());
                if (!crashed)
                {
                    router_->on_service_remove(id);
                }
                servicenum_.store(static_cast<uint32_t>(services_.size()));
                router_->make_response(sender, "service destroy"sv, response_content, respid);
                CONSOLE_INFO(router_->logger(), "[WORKER %d]service [%s:%u] destroy", workerid(), s->name().data(), s->id());
                services_.erase(iter);

                auto m = message::create();
                m->set_header("exit");
                m->set_type(PTYPE_SYSTEM);
                if (crashed)
                {
                    m->write_string("service crashed");
                }
                else
                {
                    m->write_string("service exit");
                }
                router_->broadcast(id, m);
            }
            else
            {
                router_->make_response(sender, "worker::remove_service "sv, "service not found"sv, respid, PTYPE_ERROR);
            }

            if (services_.size() == 0 && (state_.load() == state::stopping))
            {
                state_.store(state::exited, std::memory_order_release);
            }
        });
    }

    asio::io_context & worker::io_service()
    {
        return io_ctx_;
    }

    void worker::send(const message_ptr_t & msg)
    {
        if (mqueue_.push_back(std::move(msg)) == 1)
        {
            post([this]() {
                auto begin_time = time::millsecond();
                size_t count = 0;
                if (mqueue_.size() != 0)
                {
                    service* ser = nullptr;
                    swapqueue_.clear();
                    mqueue_.swap(swapqueue_);
                    count = swapqueue_.size();
                    for (auto& msg : swapqueue_)
                    {
                        handle_one(ser, msg);
                    }
                }
                auto difftime = time::millsecond() - begin_time;
                work_time_ += difftime;
                if (difftime > 1000)
                {
                    CONSOLE_WARN(router_->logger(), "worker handle cost %" PRId64 "ms queue size %zu", difftime, count);
                }
            });
        }
    }

    int32_t worker::workerid() const
    {
        return workerid_;
    }

    void worker::workerid(int32_t id)
    {
        workerid_ = id;
    }

    service * worker::find_service(uint32_t serviceid) const
    {
        auto iter = services_.find(serviceid);
        if (services_.end() != iter)
        {
            return iter->second.get();
        }
        return nullptr;
    }

    void worker::runcmd(uint32_t sender, const std::string & cmd, int32_t responseid)
    {
        post([this, sender, cmd, responseid] {
            auto params = moon::split<std::string>(cmd, ".");

            switch (moon::chash_string(params[0]))
            {
            case "worker"_csh:
            {
                if (auto iter = commands_.find(params[2]); iter != commands_.end())
                {
                    router_->make_response(sender, "", iter->second(params), responseid);
                }
                break;
            }
            case "service"_csh:
            {
                uint32_t serviceid = moon::string_convert<uint32_t>(params[1]);
                if (service* s = find_service(serviceid); s != nullptr)
                {
                    s->runcmd(sender, cmd, responseid);
                }
                else
                {
                    router_->make_response(sender, "worker::runcmd "sv, moon::format("runcmd:can not found service. %s", params[1].data()), responseid, PTYPE_ERROR);
                }
                break;
            }
            }
        });
    }

    void worker::shared(bool v)
    {
        shared_ = v;
    }

    bool worker::shared() const
    {
        return shared_.load();
    }

    uint32_t worker::servicenum() const
    {
        return servicenum_.load();
    }

    void worker::start()
    {
        post([this] {
            for (auto& it : services_)
            {
                it.second->start();
            }
        });
    }

    void worker::update()
    {
        //update_state is true
        if (update_state_.test_and_set(std::memory_order_acquire))
        {
            return;
        }

        post([this] {
            auto begin_time = time::millsecond();

            for (auto& it : services_)
            {
                it.second->update();
            }

            auto difftime = time::millsecond() - begin_time;
            work_time_ += difftime;

            update_state_.clear(std::memory_order_release);
        });
    }

    void worker::handle_one(service* ser, const message_ptr_t & msg)
    {
        if (msg->broadcast())
        {
            for (auto& it : services_)
            {
                auto& s = it.second;
                if (s->ok() && s->id() != msg->sender())
                {
                    s->handle_message(msg);
                }
            }
            return;
        }

        if (nullptr == ser || ser->id() != msg->receiver())
        {
            ser = find_service(msg->receiver());
            if (nullptr == ser)
            {
                msg->set_responseid(-msg->responseid());
                router_->make_response(msg->sender(), "worker::handle_one ", moon::format("[%u] attempt send to dead service [%u].",msg->sender(),msg->receiver()).data(), msg->responseid(), PTYPE_ERROR);
                return;
            }
        }
        ser->handle_message(msg);
    }

    void worker::register_commands()
    {
        {
            auto hander = [this](const std::vector<std::string>& params) {
                (void)params;
                auto response = moon::format(R"({"work_time":%lld})", work_time_);
                work_time_ = 0;
                return response;
            };
            commands_.try_emplace("worktime", hander);
        }

        {
            auto hander = [this](const std::vector<std::string>& params) {
                (void)params;
                std::string content;
                content.append("[");
                for (auto& it : services_)
                {
                    content.append(moon::format(R"({"name":%s,"serviceid":%u})", it.second->name().data(), it.second->id()));
                }
                content.append("]");
                return content;
            };
            commands_.try_emplace("services", hander);
        }
    }
}
