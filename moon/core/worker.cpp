#include "worker.h"
#include "common/time.hpp"
#include "common/string.hpp"
#include "common/hash.hpp"
#include "service.h"
#include "message.hpp"
#include "common/log.hpp"
#include "router.h"
#include "server.h"

namespace moon
{
    worker::worker(server* srv, router* r, int32_t id)
        : state_(state::init)
        , shared_(true)
        , serviceuid_(1)
        , work_time_(0)
        , workerid_(id)
        , router_(r)
        , server_(srv)
        , io_ctx_(1)
        , work_(asio::make_work_guard(io_ctx_))
        , prepare_uuid_(1)
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
        int32_t wkid = id();
        tmp |= static_cast<uint32_t>(wkid) << WORKER_ID_SHIFT;
        return tmp;
    }

    void worker::add_service(service_ptr_t&& s, uint32_t creatorid, int32_t responseid)
    {
        post([this, s=std::move(s), creatorid, responseid]() mutable {
            auto serviceid = s->id();
            auto res = services_.try_emplace(serviceid, std::move(s));
            MOON_CHECK(res.second, "serviceid repeated");
            res.first->second->ok(true);
            will_start_.push_back(serviceid);
            CONSOLE_INFO(router_->logger(), "[WORKER %d]new service [%s:%u]", id(), res.first->second->name().data(), res.first->second->id());
            
            if (0 != responseid)
            {
                router_->make_response(creatorid, "", std::to_string(serviceid), responseid);
            }
        });
    }

    void worker::remove_service(uint32_t serviceid, uint32_t sender, uint32_t respid, bool crashed)
    {
        post([this, serviceid, sender, respid, crashed]() {
            if (auto s =find_service(serviceid); nullptr != s)
            {
                s->destroy();
                if (services_.size() == 0)
                {
                    shared(true);
                }
                if (!crashed)
                {
                    router_->on_service_remove(serviceid);
                }

                auto response_content = moon::format(R"({"name":"%s","serviceid":%u})", s->name().data(), s->id());
                router_->make_response(sender, "service destroy"sv, response_content, respid);
                CONSOLE_INFO(router_->logger(), "[WORKER %d]service [%s:%u] destroy", id(), s->name().data(), s->id());
                services_.erase(serviceid);

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
                router_->make_response(sender, "worker::remove_service "sv,moon::format("service [%u] not found", serviceid), respid, PTYPE_ERROR);
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
        if (mqueue_.push_back(std::move(msg)) == 1)
        {
            post([this]() {
                auto begin_time = server_->now();;
                size_t count = 0;
                if (mqueue_.size() != 0)
                {
                    service* ser = nullptr;
                    swapqueue_.clear();
                    mqueue_.swap(swapqueue_);
                    count = swapqueue_.size();
                    for (auto& msg : swapqueue_)
                    {
                        handle_one(ser, std::move(msg));
                    }
                }
                auto difftime = server_->now() - begin_time;
                work_time_ += difftime;
                if (difftime > 1000)
                {
                    CONSOLE_WARN(router_->logger(), "worker handle cost %" PRId64 "ms queue size %zu", difftime, count);
                }
            });
        }
    }

    int32_t worker::id() const
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
            }
        });
    }

    uint32_t worker::prepare(const moon::buffer_ptr_t & buf)
    {
        auto iter = prepares_.emplace(prepare_uuid_++, buf);
        if (iter.second)
        {
            return iter.first->first;
        }
        return 0;
    }

    void worker::send_prepare(uint32_t sender, uint32_t receiver, uint32_t cacheid, const moon::string_view_t & header, int32_t responseid, uint8_t type) const
    {
        if (auto iter = prepares_.find(cacheid); iter != prepares_.end())
        {
            router_->send(sender, receiver, iter->second, header, responseid, type);
            return;
        }
        CONSOLE_DEBUG(server_->logger(), "send_prepare failed, can not find prepared data. id %s", cacheid);
    }

    worker_timer& worker::timer()
    {
        return timer_;
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
                msg->set_responseid(-msg->responseid());
                router_->make_response(msg->sender(), "worker::handle_one ", moon::format("[%u] attempt send to dead service [%u]: %s.", msg->sender(), msg->receiver(), moon::hex_string({msg->data(),msg->size()}).data()).data(), msg->responseid(), PTYPE_ERROR);
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

    void worker::update()
    {
        if (!will_start_.empty())
        {
            for (auto& sid : will_start_)
            {
                auto s = find_service(sid);
                if (nullptr != s && !s->started())
                {
                    s->start();
                }
            }
            will_start_.clear();
        }

        timer_.update();
        
        if (!prepares_.empty())
        {
            prepares_.clear();
            prepare_uuid_ = 1;
        }
    }
}
