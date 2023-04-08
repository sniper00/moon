#pragma once
#include "config.hpp"
#include "common/concurrent_queue.hpp"
#include "network/socket.h"

namespace moon
{
    class server;

    class worker
    {
        using queue_type = concurrent_queue<message, std::mutex, std::vector>;

        using asio_work_type = asio::executor_work_guard<asio::io_context::executor_type>;
    public:
        friend class server;

        friend class socket;

        explicit worker(server* srv,  uint32_t id);

        ~worker();

        worker(const worker&) = delete;

        worker& operator=(const worker&) = delete;

        void remove_service(uint32_t serviceid, uint32_t sender, uint32_t sessionid);

        asio::io_context& io_context();

        uint32_t id() const;

        void new_service(std::unique_ptr<service_conf> conf);

        void send(message&& msg);

        void shared(bool v);

        bool shared() const;

        moon::socket& socket() { return *socket_; }

        uint32_t alive();
    private:
        void run();

        void stop();

        void wait();

        void handle_one(service*& ser, message&& msg);

        service* find_service(uint32_t serviceid) const;
    private:
        std::atomic_bool shared_ = true;
        std::atomic_uint32_t count_ = 0;
        std::atomic_uint32_t mqsize_ = 0;
        uint32_t nextid_ = 0;
        uint32_t workerid_ = 0;
        uint32_t version_ = 0;
        double cpu_cost_ = 0.0;
        server*  server_;
        asio::io_context io_ctx_;
        asio_work_type work_;
        std::thread thread_;
        queue_type mq_;
        queue_type::container_type swapmq_;
        std::unique_ptr<moon::socket> socket_;
        std::unordered_map<uint32_t, service_ptr_t> services_;
    };
};


