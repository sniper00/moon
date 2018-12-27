#include "server.h"
#include "worker.h"

namespace moon
{
    server::server()
        :state_(state::init)
        , now_(0)
        , workers_()
        , default_log_()
        , router_(workers_, &default_log_)
    {
    }

    server::~server()
    {
        stop();
        wait();
    }

    void server::init(int worker_num, const std::string& logpath)
    {
        worker_num = (worker_num <= 0) ? 1 : worker_num;

        logger()->init(logpath);

        router_.set_server(this);

        CONSOLE_INFO(logger(), "INIT with %d workers.", worker_num);

        for (int i = 0; i != worker_num; i++)
        {
            workers_.emplace_back(std::make_unique<worker>(this, &router_,i+1));
        }

        for (auto& w : workers_)
        {
            w->run();
        }

        state_.store(state::ready);
    }

    void server::run()
    {
        if (0 == workers_.size())
        {
            printf("should run server::init first!\r\n");
            return;
        }

        for (auto& w : workers_)
        {
            w->start();
        }

        int64_t previous_tick = time::steady_millsecond();
        int64_t sleep_duration = 0;
        while (true)
        {
            now_ = time::steady_millsecond();
            auto diff = (now_ - previous_tick);
            diff = (diff < 0) ? 0 : diff;
            previous_tick = now_;

            size_t stoped_worker_num = 0;

            for (auto& w : workers_)
            {
                if (w->stoped())
                {
                    stoped_worker_num++;
                }
                w->post_update();
            }

            if (stoped_worker_num == workers_.size())
            {
                break;
            }

            if (diff <= UPDATE_INTERVAL + sleep_duration)
            {
                sleep_duration = UPDATE_INTERVAL + sleep_duration - diff;
                thread_sleep(sleep_duration);
            }
            else
            {
                sleep_duration = 0;
            }
        }
        wait();
    }

    void server::stop()
    {
        if (state_.load() != state::ready)
        {
            return;
        }

        for (auto iter = workers_.rbegin(); iter != workers_.rend(); ++iter)
        {
            (*iter)->stop();
        }
    }

    log* server::logger()
    {
        return &default_log_;
    }

    router* server::get_router()
    {
        return &router_;
    }

    size_t server::workernum() const
    {
        return workers_.size();
    }

    void server::wait()
    {
        auto state_ready = state::ready;
        if (!state_.compare_exchange_strong(state_ready, state::stopping))
        {
            return;
        }

        for (auto iter = workers_.rbegin(); iter != workers_.rend(); ++iter)
        {
            (*iter)->wait();
        }
        CONSOLE_INFO(logger(), "STOP");
        default_log_.wait();
        state_.store(state::exited);
    }

    bool server::stoped()
    {
        return state_.load() == state::exited;
    }

    int64_t server::now()
    {
        return now_;
    }
}


