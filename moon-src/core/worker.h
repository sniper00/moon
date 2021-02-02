#pragma once
#include "config.hpp"
#include "common/concurrent_queue.hpp"
#include "common/spinlock.hpp"
#include "common/timer.hpp"
#include "network/socket.h"

namespace moon
{
    class server;
    class router;

    class worker
    {
        using queue_t = concurrent_queue<message_ptr_t, std::mutex, std::vector>;

        using command_hander_t = std::function<std::string(const std::vector<std::string>&)>;

        using asio_work_t = asio::executor_work_guard<asio::io_context::executor_type>;

        class timer_expire_policy
        {
        public:
            timer_expire_policy(uint32_t serviceid, worker* wk)
                :serviceid_(serviceid), worker_(wk){}

            void operator()(timer_t id, bool last)
            {
                worker_->on_timer(id, serviceid_, last);
            }
        private:
            uint32_t serviceid_;
            worker* worker_;
        };

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
            , std::string_view header
            , int32_t sessionid
            , uint8_t type) const;

        timer_t repeat(int64_t interval, int32_t times, uint32_t serviceid);

        void remove_timer(timer_t id);
    
        moon::socket& socket() { return *socket_; }

        std::string info();
    private:
        void run();

        void stop();

        void wait();

        bool stoped() const;
    private:
        void update();

        void handle_one(service*& ser, message_ptr_t&& msg);

        service* find_service(uint32_t serviceid) const;

        void on_timer(timer_t timerid, uint32_t serviceid, bool last);
    private:
        std::atomic<state> state_ = state::init;
        std::atomic_bool shared_ = true;
        //to prevent post too many update event
        std::atomic_flag update_state_ = ATOMIC_FLAG_INIT;
        std::atomic_uint32_t count_ = 0;
        uint32_t uuid_ = 0;
        int64_t cpu_cost_ = 0;
        std::atomic_int32_t mqsize_ = 0;
        uint32_t workerid_;
        router*  router_;
        server*  server_;
        asio::io_context io_ctx_;
        asio_work_t work_;
        std::thread thread_;
        queue_t mq_;
        queue_t::container_type swapmq_;
        base_timer<timer_expire_policy> timer_;
        std::unique_ptr<moon::socket> socket_;
        std::unordered_map<uint32_t, service_ptr_t> services_;
        std::unordered_map<uint32_t, moon::buffer_ptr_t> prefabs_;
        std::unordered_map<std::string, command_hander_t> commands_;
    };
};


