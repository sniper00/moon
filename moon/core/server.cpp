/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#include "server.h"
#include "worker.h"

namespace moon
{
    server::server()
		:state_(state::init)
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

    void server::init(uint8_t worker_num, const std::string& logpath)
    {
        worker_num == 0 ? 1 : worker_num;

		logger()->init(logpath);

		router_.set_stop([this]() {
			stop();
		});

		CONSOLE_INFO(logger(), "INIT with %d workers.", worker_num);

        for (uint8_t i = 0; i != worker_num; i++)
        {
			auto& w =workers_.emplace_back(new worker(&router_));
            w->workerid(i+1);
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

        int64_t prew_tick = time::millsecond();
        int64_t prev_sleep_time = 0;
        while (true)
        {
            auto now = time::millsecond();
            auto diff = (now - prew_tick);
            prew_tick = now;

            int stoped_worker_num = 0;

            for (auto w : workers_)
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

       wait();
    }

    void server::stop()
    {
		if (state_.load()!=state::ready)
		{
			return;
		}

		for (int i = 0; i < 100; i++)
		{
			for (auto iter = workers_.rbegin(); iter != workers_.rend(); ++iter)
			{
				(*iter)->stop();
			}
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
		if (!state_.compare_exchange_strong(state_ready, state::exited))
		{
			return;
		}

		for (auto iter = workers_.rbegin(); iter != workers_.rend(); ++iter)
		{
			(*iter)->wait();
			delete (*iter);
		}

		CONSOLE_INFO(logger(), "STOP");
		default_log_.wait();
	}
}


