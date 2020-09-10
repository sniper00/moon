#include "server.h"
#include "worker.h"

namespace moon
{
    server::~server()
    {
        stop(-1);
        wait();
    }

    void server::init(int worker_num, const std::string& logpath)
    {
        worker_num = (worker_num <= 0) ? 1 : worker_num;

        logger_.init(logpath);

        router_.init(this);

        CONSOLE_INFO(logger(), "INIT with %d workers.", worker_num);

        for (int i = 0; i != worker_num; i++)
        {
            workers_.emplace_back(std::make_unique<worker>(this, &router_, i + 1));
        }

        for (auto& w : workers_)
        {
            w->run();
        }

        state_.store(state::init, std::memory_order_release);
    }

    void server::run(size_t count)
    {
        //wait all bootstrap services created
        while (!signalcode_ && service_count() < count)
        {
            thread_sleep(10);
        }

        if (signalcode_)
        {
            wait();
            return;
        }

        state_.store(state::ready, std::memory_order_release);

        //call services's start callback
        for (auto& w : workers_)
        {
            w->start();
        }

        int64_t prev = time::now();
        int64_t sleep_duration = 0;
        while (true)
        {
            state old = state::ready;
            if (signalcode_ && state_.compare_exchange_strong(old
                , state::stopping
                , std::memory_order_acquire))
            {
                CONSOLE_WARN(logger(), "Received signal code %d", signalcode_);
                for (auto iter = workers_.rbegin(); iter != workers_.rend(); ++iter)
                {
                    (*iter)->stop();
                }
            }

            now_ = time::now();

            auto delta = (now_ - prev);
            delta = (delta < 0) ? 0 : delta;
            prev = now_;

            size_t stoped_num = 0;

            for (const auto& w : workers_)
            {
                if (w->stoped())
                {
                    stoped_num++;
                }
                w->update();
            }

            if (stoped_num == workers_.size())
            {
                break;
            }

            if (delta <= UPDATE_INTERVAL + sleep_duration)
            {
                sleep_duration = UPDATE_INTERVAL + sleep_duration - delta;
                thread_sleep(sleep_duration);
            }
            else
            {
                sleep_duration = 0;
            }
        }
        wait();
    }

    void server::stop(int  signalcode)
    {
        signalcode_ = signalcode;
    }

    log* server::logger()
    {
        return &logger_;
    }

    router* server::get_router()
    {
        return &router_;
    }

    void server::wait()
    {
        for (auto iter = workers_.rbegin(); iter != workers_.rend(); ++iter)
        {
            (*iter)->wait();
        }
        CONSOLE_INFO(logger(), "STOP");
        logger_.wait();
        state_.store(state::exited, std::memory_order_release);
    }

    state server::get_state()
    {
        return state_.load(std::memory_order_acquire);
    }

    int64_t server::now(bool sync)
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

    uint32_t server::service_count()
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
        uint32_t  n = next_.fetch_add(1);
        std::vector<uint32_t> free_worker;
        for (auto& w : workers_)
        {
            if (w->shared())
            {
                free_worker.push_back(w->id() - 1U);
            }
        }
        if (!free_worker.empty())
        {
            auto wkid = free_worker[n % free_worker.size()];
            return workers_[wkid].get();
        }
        n %= workers_.size();
        return workers_[n].get();
    }

    worker* server::get_worker(uint32_t workerid) const
    {
        if ((workerid <= 0 || workerid > static_cast<uint32_t>(workers_.size())))
        {
            return nullptr;
        }

        --workerid;
        return workers_[workerid].get();
    }

    std::vector<std::unique_ptr<worker>>& server::get_workers()
    {
        return workers_;
    }
}


