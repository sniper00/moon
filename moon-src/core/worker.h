#pragma once
#include "config.hpp"
#include "common/concurrent_queue.hpp"
#include "common/spinlock.hpp"
#include "worker_timer.hpp"
#include "network/socket.h"

namespace moon
{
    class server;
    class router;

    class worker
    {
        using queue_t = concurrent_queue<message_ptr_t, moon::spin_lock, std::vector>;

        using command_hander_t = std::function<std::string(const std::vector<std::string>&)>;

        using asio_work_t = asio::executor_work_guard<asio::io_context::executor_type>;
    public:
        static constexpr uint16_t max_uuid = 0xFFFF;

        friend class server;

        friend class socket;

        explicit worker(server* srv, router* r, uint32_t id);

        ~worker();

        worker(const worker&) = delete;

        worker& operator=(const worker&) = delete;

        void remove_service(uint32_t serviceid, uint32_t sender, uint32_t sessionid);

        asio::io_context& io_context();

        uint32_t id() const;

        size_t size() const;

        uint32_t uuid();

        void add_service(std::string service_type
            , std::string config
            , bool unique
            , uint32_t creatorid
            , int32_t sessionid);

        void send(message_ptr_t&& msg);

        void shared(bool v);

        bool shared() const;

        void runcmd(uint32_t sender, const std::string& cmd, int32_t sessionid);

        uint32_t make_prefab(const moon::buffer_ptr_t & buf);

        void send_prefab(uint32_t sender
            , uint32_t receiver
            , uint32_t prefabid
            , string_view_t header
            , int32_t sessionid
            , uint8_t type) const;
    
        worker_timer& timer() { return timer_; }

        moon::socket& socket() { return *socket_; }

        template<typename THandler>
        void post(THandler&& h)
        {
            asio::post(io_ctx_, std::forward<THandler>(h));
        }
    private:
        void run();

        void stop();

        void wait();

        bool stoped();
    private:
        void start();

        void post_update();

        void handle_one(service*& ser, message_ptr_t&& msg);

        void register_commands();

        void update();

        void check_start();

        service* find_service(uint32_t serviceid) const;
    private:
        std::atomic<state> state_ = state::init;
        std::atomic_bool shared_ = true;
        //to prevent post too many update event
        std::atomic_flag update_state_ = ATOMIC_FLAG_INIT;
        uint32_t uuid_ = 0;
        int64_t cpu_time_ = 0;
        uint32_t workerid_;
        router*  router_;
        server*  server_;
        asio::io_context io_ctx_;
        asio_work_t work_;
        std::thread thread_;
        queue_t mq_;
        queue_t::container_type swapmq_;
        worker_timer timer_;
        std::unique_ptr<moon::socket> socket_;
        std::vector<uint32_t> will_start_;
        std::unordered_map<uint32_t, service_ptr_t> services_;
        std::unordered_map<std::string, command_hander_t> commands_;
        std::unordered_map<uint32_t, moon::buffer_ptr_t> prefabs_;
    };
};


