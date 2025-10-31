#pragma once
#include "common/concurrent_queue.hpp"
#include "config.hpp"
#include "network/socket_server.h"

namespace moon {
class server;

class worker {
    using queue_type = concurrent_queue<message, std::mutex, std::vector>;

    using asio_work_type = asio::executor_work_guard<asio::io_context::executor_type>;

public:
    friend class socket_server;

    explicit worker(server* srv, uint32_t id);

    ~worker();

    worker(const worker&) = delete;

    worker& operator=(const worker&) = delete;

    asio::io_context& io_context();

    uint32_t id() const;

    void new_service(std::unique_ptr<service_conf> conf);

    void remove_service(uint32_t serviceid, uint32_t sender, int64_t sessionid);

    void scan(uint32_t sender, int64_t sessionid);

    void send(message&& msg);

    void shared(bool v);

    bool shared() const;

    moon::socket_server& socket_server() {
        return *socket_server_;
    }

    size_t mq_size() const {
        return mq_.size() + swapped_size_.load(std::memory_order_relaxed);
    }

    uint32_t alive();

    double cpu() {
        return std::exchange(cpu_, 0.0);
    }

    uint32_t count() const {
        return count_.load(std::memory_order_relaxed);
    }

    void run();

    void stop();

    void wait();

    void signal(int val) const;

private:
    service* handle_one(service* s, message&& msg);

    service* find_service(uint32_t serviceid) const;

    uint32_t allocate_service_id(uint32_t opt_service_id);

private:
    std::atomic_bool shared_ = true;
    std::atomic_uint32_t count_ = 0;
    std::atomic_size_t swapped_size_ = 0;
    uint32_t nextid_ = 0;
    uint32_t workerid_ = 0;
    uint32_t version_ = 0;
    double cpu_ = 0.0;
    server* server_;
    std::atomic<service*> current_ = nullptr;
    asio::io_context io_ctx_;
    asio_work_type work_;
    std::thread thread_;
    queue_type mq_;
    std::unique_ptr<moon::socket_server> socket_server_;
    std::unordered_map<uint32_t, service_ptr_t> services_;
};
}; // namespace moon
