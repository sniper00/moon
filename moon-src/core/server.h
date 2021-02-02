#pragma once
#include "config.hpp"
#include "common/log.hpp"
#include "common/time.hpp"
#include "router.h"
#include "worker.h"

namespace moon
{
    class  server final
    {
    public:
        server() = default;

        ~server();

        server(const server&) = delete;

        server& operator=(const server&) = delete;

        server(server&&) = delete;

        void init(int worker_num, const std::string& logpath);

        void run();

        void stop(int signalcode);

        log* logger();

        router* get_router();

        state get_state();

        std::time_t now(bool sync = false);

        uint32_t service_count() const;

        worker* next_worker();

        worker* get_worker(uint32_t workerid) const;

        std::vector<std::unique_ptr<worker>>& get_workers();
    private:
        void wait();
    private:
        volatile int signalcode_ = 0;
        std::atomic<state> state_ = state::unknown;
        std::atomic<uint32_t> next_ = 0;
        std::time_t now_ = 0;
        log logger_;
        router router_;
        std::vector<std::unique_ptr<worker>> workers_;
    };
};


