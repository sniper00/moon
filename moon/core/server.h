#pragma once
#include "config.h"
#include "router.h"
#include "common/log.hpp"

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

        void run();

        void stop();

        log* logger();

        router* get_router();

        size_t workernum() const;

        state get_state();

        int64_t now();
    private:
        void wait();
    private:
        std::atomic<state> state_;
        int64_t now_;
        std::vector<std::unique_ptr<worker>> workers_;
        log default_log_;
        router router_;
    };
};


