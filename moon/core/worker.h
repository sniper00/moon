#pragma once
#include "config.h"
#include "asio.hpp"
#include "common/concurrent_queue.hpp"
#include "common/spinlock.hpp"
#include "worker_timer.hpp"

namespace moon
{
    class server;
    class router;

    class worker
    {
    public:
        static const uint16_t MAX_SERVICE_NUM = 0xFFFF;

        friend class server;

        explicit worker(server* srv, router* r, int32_t id);

        ~worker();

        worker(const worker&) = delete;

        worker& operator=(const worker&) = delete;

        void remove_service(uint32_t serviceid, uint32_t sender, uint32_t respid, bool crashed = false);

        asio::io_context& io_context();

        int32_t id() const;

        size_t size() const;

        uint32_t make_serviceid();

        void add_service(service_ptr_t&& s, uint32_t creatorid, int32_t responseid);

        void send(message_ptr_t&& msg);

        void shared(bool v);

        bool shared() const;

        service* find_service(uint32_t serviceid) const;

        void runcmd(uint32_t sender, const std::string& cmd, int32_t responseid);

        uint32_t prepare(const moon::buffer_ptr_t & buf);

        void send_prepare(uint32_t sender, uint32_t receiver, uint32_t cacheid, const  moon::string_view_t& header, int32_t responseid, uint8_t type) const;
    
        worker_timer& timer();
    private:
        void run();

        void stop();

        void wait();

        bool stoped();

        template<typename THandler>
        void post(THandler&& h)
        {
            asio::post(io_ctx_, std::forward<THandler>(h));
        }
    private:
        void start();

        void post_update();

        void handle_one(service*& ser, message_ptr_t&& msg);

        void register_commands();

        void update();
    private:
        //To prevent post too many update event
        std::atomic_flag update_state_ = ATOMIC_FLAG_INIT;
        std::atomic<state> state_;
        std::atomic_bool shared_;
        std::atomic<uint16_t> serviceuid_;
        int32_t workerid_;
        int64_t work_time_;
        router*  router_;
        server*  server_;
        std::thread thread_;
        asio::io_context io_ctx_;
        asio::executor_work_guard<asio::io_context::executor_type> work_;
        std::unordered_map<uint32_t, service_ptr_t> services_;

        using queue_t = concurrent_queue<message_ptr_t, moon::spin_lock, std::vector>;
        queue_t::container_type swapqueue_;
        queue_t mqueue_;

        using command_hander_t = std::function<std::string(const std::vector<std::string>&)>;
        std::unordered_map<std::string, command_hander_t> commands_;

        worker_timer timer_;

        uint32_t prepare_uuid_;
        std::unordered_map<uint32_t, moon::buffer_ptr_t> prepares_;

        std::vector<uint32_t> will_start_;
    };
};


