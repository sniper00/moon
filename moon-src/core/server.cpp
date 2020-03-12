#include "server.h"
#include "worker.h"

namespace moon
{
    server::server()
        :state_(state::unknown)
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
            workers_.emplace_back(std::make_unique<worker>(this, &router_, i + 1));
        }

        for (auto& w : workers_)
        {
            w->run();
        }

        state_.store(state::init, std::memory_order_release);
    }

    void server::run()
    {
        if (0 == workers_.size())
        {
            printf("should run server::init first!\r\n");
            return;
        }

        state_.store(state::ready, std::memory_order_release);

        for (auto& w : workers_)
        {
            w->start();
        }

        int64_t previous_tick = time::now();
        int64_t sleep_duration = 0;
        int64_t total_ = time::now()%1000;
        while (true)
        {
            now_ = time::now();

            auto diff = (now_ - previous_tick);
            diff = (diff < 0) ? 0 : diff;
            previous_tick = now_;

            total_ += diff;

            //datetime update on seconds
            if (total_ >= 1000)
            {
                total_ -= 1000;
                datetime_.update(now_/1000);
            }

            size_t stoped_worker_num = 0;

            for (const auto& w : workers_)
            {
                if (w->stoped())
                {
                    stoped_worker_num++;
                }
                w->update();
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
        if (state_.exchange(state::stopping, std::memory_order_acquire) > state::ready)
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

    void server::wait()
    {
        for (auto iter = workers_.rbegin(); iter != workers_.rend(); ++iter)
        {
            (*iter)->wait();
        }
        CONSOLE_INFO(logger(), "STOP");
        default_log_.wait();
        state_.store(state::exited, std::memory_order_release);
    }

    state server::get_state()
    {
        return state_.load();
    }

    int64_t server::now()
    {
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

    datetime & server::get_datetime()
    {
        return datetime_;
    }
}


