#include "server.h"
#include "worker.h"
#include "message.hpp"
#include "common/hash.hpp"

namespace moon
{
    static uint32_t worker_id(uint32_t serviceid)
    {
        return ((serviceid >> WORKER_ID_SHIFT) & 0xFF);
    }

    server::~server()
    {
        wait();
    }

    void server::init(int worker_num, const std::string& logfile)
    {
        worker_num = (worker_num <= 0) ? 1 : worker_num;

        logger_.init(logfile);

        CONSOLE_INFO(logger(), "INIT with %d workers.", worker_num);

        for (int i = 0; i != worker_num; i++)
        {
            workers_.emplace_back(std::make_unique<worker>(this,  i + 1));
        }

        for (auto& w : workers_)
        {
            w->run();
        }

        state_.store(state::init, std::memory_order_release);
    }

    void server::run()
    {
        asio::io_context io_context;
        asio::steady_timer timer(io_context);
        asio::error_code ignore;

        state_.store(state::ready, std::memory_order_release);
        while (true)
        {
            now_ = time::now();

            if (signalcode_ < 0 )
            {
                break;
            }

            state old = state::ready;
            if (signalcode_>0 && state_.compare_exchange_strong(old
                , state::stopping
                , std::memory_order_acquire))
            {
                CONSOLE_WARN(logger(), "Received signal code %d", signalcode_);
                for (auto iter = workers_.rbegin(); iter != workers_.rend(); ++iter)
                {
                    (*iter)->stop();
                }
            }

            size_t alive = workers_.size();
            for (const auto& w : workers_)
            {
                if (w->stoped())
                {
                    --alive;
                }
            }

            if (0 == alive)
            {
                break;
            }
            timer_.update(now_);
            timer.expires_after(std::chrono::milliseconds(1));
            timer.wait(ignore);
        }
        wait();
    }

    void server::stop(int  signalcode)
    {
        signalcode_ = signalcode;
    }

    log* server::logger() const
    {
        return &logger_;
    }

    void server::wait()
    {
        for (auto iter = workers_.rbegin(); iter != workers_.rend(); ++iter)
        {
            (*iter)->wait();
        }
        CONSOLE_INFO(logger(), "STOP");
        logger_.wait();
        state_.store(state::stopped, std::memory_order_release);
    }

    state server::get_state() const
    {
        return state_.load(std::memory_order_acquire);
    }

    std::time_t server::now(bool sync)
    {
        if (sync)
        {
            now_ = time::now();
        }

        if (now_ == 0)
        {
            return time::now();
        }
        return now_;
    }

    uint32_t server::service_count() const
    {
        uint32_t res = 0;
        for (const auto& w : workers_)
        {
            res += w->count_.load(std::memory_order_acquire);
        }
        return res;
    }

    worker* server::next_worker()
    {
        uint32_t  idx = next_.fetch_add(1);
        uint32_t free_count = 0;
        for (const auto& w : workers_)
        {
            if (w->shared())
            {
                ++free_count;
            }
        }

        if (free_count>0)
        {
            idx = idx % free_count;
            for (const auto& w : workers_)
            {
                if (!w->shared())
                    continue;
                if (idx == 0)
                {
                    return w.get();
                }
                idx--;
            }
        }
        idx %= workers_.size();
        return workers_[idx].get();
    }

    worker* server::get_worker(uint32_t workerid, uint32_t serviceid) const
    {
        if (workerid == 0)
        {
            workerid = worker_id(serviceid);
        }

        if ((workerid <= 0 || workerid > static_cast<uint32_t>(workers_.size())))
        {
            return nullptr;
        }

        --workerid;
        return workers_[workerid].get();
    }

    void server::timeout(int64_t interval, uint32_t serviceid, uint32_t timerid)
    {
        return timer_.add(now_+ interval, serviceid, timerid, this);
    }

    void server::on_timer(uint32_t serviceid, uint32_t timerid)
    {
        auto msg = message::create(buffer_ptr_t{nullptr});
        msg->set_type(PTYPE_TIMER);
        msg->set_sender(timerid);
        msg->set_receiver(serviceid);
        send_message(std::move(msg));
    }

    void server::new_service(std::string service_type, std::string config, bool unique, int32_t workerid, uint32_t creatorid, int32_t sessionid)
    {
        worker* w = get_worker(workerid);
        if (nullptr != w)
        {
            w->shared(false);
        }
        else
        {
            w = next_worker();
        }
        w->add_service(std::move(service_type), std::move(config), unique, creatorid, sessionid);
    }

    void server::remove_service(uint32_t serviceid, uint32_t sender, int32_t sessionid)
    {
        worker* w = get_worker(0, serviceid);
        if (nullptr != w)
        {
            w->remove_service(serviceid, sender, sessionid);
        }
        else
        {
            auto content = moon::format("worker %u not found.", serviceid);
            response(sender, "router::remove_service "sv, content, sessionid, PTYPE_ERROR);
        }
    }

    void server::scan_services(uint32_t sender, uint32_t workerid, int32_t sessionid)
    {
        auto* w = get_worker(workerid);
        if (nullptr == w)
        {
            return;
        }
        asio::post(w->io_context(), [this, sender, w, sessionid] {
            std::string content;
            content.append("[");
            for (auto& it : w->services_)
            {
                content.append(moon::format(
                    R"({"name":"%s","serviceid":"%X"},)",
                    it.second->name().data(),
                    it.second->id()));
            }
            content.back() = ']';
            response(sender, std::string_view{}, content, sessionid);
         });
    }

    bool server::send_message(message_ptr_t&& m) const
    {
        worker* w = get_worker(0, m->receiver());
        if (nullptr == w)
        {
            CONSOLE_ERROR(logger(), "invalid message receiver serviceid %X", m->receiver());
            return false;
        }
        w->send(std::move(m));
        return true;
    }

    bool server::send(uint32_t sender, uint32_t receiver, buffer_ptr_t data, std::string_view header, int32_t sessionid, uint8_t type) const
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
        return send_message(std::move(m));
    }

    void server::broadcast(uint32_t sender, const buffer_ptr_t& buf, std::string_view header, uint8_t type) const
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

    bool server::register_service(const std::string& type, register_func f)
    {
        auto ret = regservices_.emplace(type, f);
        MOON_ASSERT(ret.second
            , moon::format("already registed service type[%s].", type.data()).data());
        return ret.second;
    }

    service_ptr_t server::make_service(const std::string& type)
    {
        auto iter = regservices_.find(type);
        if (iter != regservices_.end())
        {
            return iter->second();
        }
        return nullptr;
    }

    std::string server::get_env(const std::string& name) const
    {
        std::string v;
        if (env_.try_get_value(name, v))
        {
            return v;
        }
        return std::string{};
    }

    void server::set_env(std::string name, std::string value)
    {
        env_.set(std::move(name), std::move(value));
    }

    uint32_t server::get_unique_service(const std::string& name) const
    {
        if (name.empty())
        {
            return 0;
        }
        uint32_t id = 0;
        unique_services_.try_get_value(name, id);
        return id;
    }

    bool server::set_unique_service(std::string name, uint32_t v)
    {
        if (name.empty())
        {
            return false;
        }
        return unique_services_.try_set(std::move(name), v);
    }

    void server::response(uint32_t to, std::string_view header, std::string_view content, int32_t sessionid, uint8_t mtype)
    {
        if (to == 0 || sessionid == 0)
        {
            if (get_state() == state::ready && mtype == PTYPE_ERROR && !content.empty())
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

    std::string server::worker_info(uint32_t workerid)
    {
        worker* w = get_worker(workerid);
        if (nullptr == w)
        {
            return std::string{};
        }
        return w->info();
    }
}
