#pragma once
#include "config.hpp"
#include "router.h"
#include "common/log.hpp"
#include "common/time.hpp"

namespace moon
{
    class  server final
    {
    public:
        server();

        ~server();

        server(const server&) = delete;

        server& operator=(const server&) = delete;

        server(server&&) = delete;

        void init(int worker_num, const std::string& logpath);

        void run(size_t count);

        void stop();

        log* logger();

        router* get_router();

        state get_state();

        int64_t now();

        uint32_t service_count();

        datetime& get_datetime();
    private:
        void wait();
    private:
        volatile bool signalstop_ = false;
        std::atomic<state> state_ = state::unknown;
        int64_t now_ = 0;
        std::vector<std::unique_ptr<worker>> workers_;
        log logger_;
        router router_;
        datetime datetime_;
    };
};


