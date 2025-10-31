#pragma once
#include "common/concurrent_map.hpp"
#include "common/timer.hpp"
#include "config.hpp"
#include "log.hpp"
#include "worker.h"

namespace moon {
class server final {
    class timer_expire_policy {
    public:
        timer_expire_policy() = default;

        timer_expire_policy(uint32_t serviceid, int64_t timerid, server* srv):
            serviceid_(serviceid),
            timerid_(timerid),
            server_(srv) {}

        void operator()() const {
            server_->on_timer(serviceid_, timerid_);
        }

        int64_t id() const {
            return timerid_;
        }

    private:
        uint32_t serviceid_ = 0;
        int64_t timerid_ = 0;
        server* server_ = nullptr;
    };

public:
    using register_func = service_ptr_t (*)();

    using timer_type = base_timer<timer_expire_policy>;

    server() = default;

    ~server();

    server(const server&) = delete;

    server& operator=(const server&) = delete;

    server(server&&) = delete;

    void init(uint32_t worker_num);

    int run();

    void stop(int exitcode);

    state get_state() const;

    void set_state(state st);

    std::time_t now(bool sync = false);

    std::time_t now_without_offset() const {
        return now_without_offset_;
    }

    uint32_t service_count() const;

    worker* next_worker();

    worker* get_worker(uint32_t workerid, uint32_t serviceid = 0) const;

    void timeout(int64_t interval, uint32_t serviceid, int64_t timerid);

    void new_service(std::unique_ptr<service_conf> conf);

    void remove_service(uint32_t serviceid, uint32_t sender, int64_t sessionid) const;

    bool scan_services(uint32_t sender, uint32_t workerid, int64_t sessionid) const;

    bool send_message(message&& msg) const;

    bool send(uint32_t sender, uint32_t receiver, buffer_ptr_t buf, int64_t sessionid, uint8_t type)
        const;

    void broadcast(uint32_t sender, const buffer& buf, uint8_t type) const;

    bool register_service(const std::string& type, register_func func);

    service_ptr_t make_service(const std::string& type);

    std::shared_ptr<const std::string> get_env(const std::string& name) const;

    void set_env(std::string name, std::string value);

    uint32_t get_unique_service(const std::string& name) const;

    bool set_unique_service(std::string name, uint32_t v);

    //Used to respond to calls from the user layer to the framework layer
    void
    response(uint32_t to, std::string_view content, int64_t sessionid, uint8_t mtype = PTYPE_TEXT)
        const;

    std::string info() const;

    uint32_t nextfd();

    bool try_lock_fd(uint32_t fd);

    void unlock_fd(uint32_t fd);

    size_t socket_num() const;

    static uint32_t worker_id(uint32_t serviceid) {
        return ((serviceid >> WORKER_ID_SHIFT) & 0xFF);
    }

private:
    void on_timer(uint32_t serviceid, int64_t timerid) const;

    void wait();

private:
    std::atomic_int32_t exitcode_ = std::numeric_limits<int>::max();
    std::atomic<state> state_ = state::unknown;
    std::atomic<uint32_t> fd_seq_ = 1;
    std::time_t now_ = 0;
    std::time_t now_without_offset_ = 0;
    mutable std::mutex fd_lock_;
    std::unordered_map<std::string, register_func> regservices_;
    concurrent_map<std::string, std::shared_ptr<const std::string>, rwlock> env_;
    concurrent_map<std::string, uint32_t, rwlock> unique_services_;
    std::unordered_set<uint32_t> fd_watcher_;
    std::vector<std::unique_ptr<timer_type>> timer_;
    std::vector<std::unique_ptr<worker>> workers_;
};
}; // namespace moon
