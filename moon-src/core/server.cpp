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

    void server::run(size_t count)
    {
        //wait all bootstrap service created
        while (!signalstop_ && service_count() < count)
        {
            thread_sleep(10);
        }

        if (signalstop_)
        {
            return;
        }

        state_.store(state::ready, std::memory_order_release);

        // then call services's start callback
        for (auto& w : workers_)
        {
            w->start();
        }

        int64_t prev = time::now();
        int64_t sleep_duration = 0;
        int64_t datetime_tick = time::now() % 1000;
        while (true)
        {
            state old = state::ready;
            if (signalstop_ && state_.compare_exchange_strong(old
                , state::stopping
                , std::memory_order_acquire))
            {
                for (auto iter = workers_.rbegin(); iter != workers_.rend(); ++iter)
                {
                    (*iter)->stop();
                }
            }

            now_ = time::now();

            auto delta = (now_ - prev);
            delta = (delta < 0) ? 0 : delta;
            prev = now_;

            datetime_tick += delta;

            //datetime update on seconds
            if (datetime_tick >= 1000)
            {
                datetime_tick -= 1000;
                datetime_.update(now_ / 1000);
            }

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

    void server::stop()
    {
        signalstop_ = true;
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
        return state_.load(std::memory_order_acquire);
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

    datetime& server::get_datetime()
    {
        return datetime_;
    }
}


