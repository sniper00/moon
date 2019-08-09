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
    }

    void worker::run()
    {
        register_commands();

        timer_.set_now_func([this]() {
            return server_->now();
        });

        timer_.set_on_timer([this](timer_id_t timerid, uint32_t serviceid, bool remove) {
            if (auto s = find_service(serviceid); nullptr != s)
            {
                s->on_timer(timerid, remove);
            }
            else
            {
                timer_.remove(timerid);
            }
        });

        socket_ = std::make_unique<moon::socket>(router_, this, io_ctx_);

        thread_ = std::thread([this]() {
            state_.store(state::ready, std::memory_order_release);
            CONSOLE_INFO(router_->logger(), "WORKER-%u START", workerid_);
            io_ctx_.run();
            CONSOLE_INFO(router_->logger(), "WORKER-%u STOP", workerid_);
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

    uint32_t worker::uuid()
    {
        auto res = uuid_++;
        res %= max_uuid;
        ++res;
        res |= id() << WORKER_ID_SHIFT;
        return res;
    }

    void worker::add_service(service_ptr_t&& s, const std::string& config, uint32_t creatorid, int32_t sessionid)
    {
        post([this, s = std::move(s), config = std::move(config), creatorid, sessionid]() mutable {
            do
            {
                if (!s->init(config))
                {
                    break;
                }

                auto serviceid = s->id();
                auto res = services_.try_emplace(serviceid, std::move(s));
                if (!res.second)
                {
                    CONSOLE_ERROR(router_->logger(), "new service[%s] failed: serviceid repeated.", s->name().data());
                    break;
                }

                res.first->second->ok(true);
                will_start_.push_back(serviceid);
                if (0 != sessionid)//only dynamically created service has sessionid
                {
                    check_start();//force service invoke start, ready to handle message
                    router_->response(creatorid, std::string_view{}, std::to_string(serviceid), sessionid);
                }
                return;
            } while (false);

            router_->on_service_remove(s->id());
            shared(services_.empty());
            if (0 != sessionid)
            {
                router_->response(creatorid, std::string_view{}, "0"sv, sessionid);
            }
        });
    }

    void worker::remove_service(uint32_t serviceid, uint32_t sender, uint32_t sessionid, bool crashed)
    {
        post([this, serviceid, sender, sessionid, crashed]() {
            if (auto s = find_service(serviceid); nullptr != s)
            {
                s->destroy();
                if (!crashed)
                {
                    router_->on_service_remove(serviceid);
                }

                auto content = moon::format(R"({"name":"%s","serviceid":%X})", s->name().data(), s->id());
                router_->response(sender, "service destroy"sv, content, sessionid);
                services_.erase(serviceid);
                if (services_.empty()) shared(true);

                string_view_t header{ "exit" };
                auto buf = message::create_buffer();
                if (crashed)
                {
                    string_view_t str{ "service crashed" };
                    buf->write_back(str.data(), str.size());
                }
                else
                {
                    string_view_t str{ "service exit" };
                    buf->write_back(str.data(), str.size());
                }
                router_->broadcast(serviceid, buf, header, PTYPE_SYSTEM);
            }
            else
            {
                router_->response(sender, "worker::remove_service "sv, moon::format("service [%X] not found", serviceid), sessionid, PTYPE_ERROR);
            }

            if (services_.size() == 0 && (state_.load() == state::stopping))
            {
                state_.store(state::exited, std::memory_order_release);
            }
        });
    }

    asio::io_context & worker::io_context()
    {
        return io_ctx_;
    }

    void worker::send(message_ptr_t&& msg)
    {
        if (mq_.push_back(std::move(msg)) == 1)
        {
            post([this]() {
                auto begin_time = server_->now();;
                size_t count = 0;
                if (mq_.size() != 0)
                {
                    service* ser = nullptr;
                    swapmq_.clear();
                    mq_.swap(swapmq_);
                    count = swapmq_.size();
                    for (auto& msg : swapmq_)
                    {
                        handle_one(ser, std::move(msg));
                    }
                }

                if (begin_time != 0)
                {
                    auto difftime = server_->now() - begin_time;
                    cpu_time_ += difftime;
                    if (difftime > 1000)
                    {
                        CONSOLE_WARN(router_->logger(), "worker handle cost %" PRId64 "ms queue size %zu", difftime, count);
                    }
                }
            });
        }
    }

    uint32_t worker::id() const
    {
        return workerid_;
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

    void worker::runcmd(uint32_t sender, const std::string & cmd, int32_t sessionid)
    {
        post([this, sender, cmd, sessionid] {
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

    uint32_t worker::make_prefab(const moon::buffer_ptr_t & buf)
    {
        auto iter = prefabs_.emplace(uuid(), buf);
        if (iter.second)
        {
            return iter.first->first;
        }
        return 0;
    }

    void worker::send_prefab(uint32_t sender, uint32_t receiver, uint32_t prefabid, const moon::string_view_t & header, int32_t sessionid, uint8_t type) const
    {
        if (auto iter = prefabs_.find(prefabid); iter != prefabs_.end())
        {
            router_->send(sender, receiver, iter->second, header, sessionid, type);
            return;
        }
        CONSOLE_DEBUG(server_->logger(), "send_prefab failed, can not find prepared data. prefabid %u", prefabid);
    }

    void worker::shared(bool v)
    {
        shared_ = v;
    }

    bool worker::shared() const
    {
        return shared_.load();
    }

    size_t worker::size() const
    {
        return services_.size();
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

    void worker::post_update()
    {
        //update_state is true
        if (update_state_.test_and_set(std::memory_order_acquire))
        {
            return;
        }

        post([this] {
            update();
            update_state_.clear(std::memory_order_release);
        });
    }

    void worker::handle_one(service*& ser, message_ptr_t&& msg)
    {
        if (msg->broadcast())
        {
            for (auto& it : services_)
            {
                auto& s = it.second;
                if (s->ok() && s->id() != msg->sender())
                {
                    s->handle_message(std::forward<message_ptr_t>(msg));
                }
            }
            return;
        }

        if (nullptr == ser || ser->id() != msg->receiver())
        {
            ser = find_service(msg->receiver());
            if (nullptr == ser)
            {
                msg->set_sessionid(-msg->sessionid());
                router_->response(msg->sender(), "worker::handle_one "sv, moon::format("[%u] attempt send to dead service [%u]: %s.", msg->sender(), msg->receiver(), moon::hex_string({ msg->data(),msg->size() }).data()).data(), msg->sessionid(), PTYPE_ERROR);
                return;
            }
        }
        ser->handle_message(std::forward<message_ptr_t>(msg));
        timer_.update();
    }

    void worker::register_commands()
    {
        {
            auto hander = [this](const std::vector<std::string>& params) {
                (void)params;
                auto response = moon::format(R"({"work_time":%lld})", cpu_time_);
                cpu_time_ = 0;
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

    void worker::update()
    {
        timer_.update();

        check_start();

        if (!prefabs_.empty())
        {
            prefabs_.clear();
        }
    }

    void worker::check_start()
    {
        if (!will_start_.empty())
        {
            for (auto sid : will_start_)
            {
                auto s = find_service(sid);
                if (nullptr != s)
                {
                    s->start();
                }
            }
            will_start_.clear();
        }
    }
}
