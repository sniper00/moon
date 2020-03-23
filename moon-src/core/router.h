#pragma once
#include "config.hpp"
#include "common/concurrent_map.hpp"
#include "common/rwlock.hpp"
#include "common/log.hpp"

namespace asio {
    class io_context;
}

namespace moon
{
    class server;
    class worker;

    class router
    {
    public:
        friend class server;

        using register_func = service_ptr_t(*)();

        router(std::vector<std::unique_ptr<worker>>& workers, log* logger);

        router(const router&) = delete;

        router& operator=(const router&) = delete;

        void new_service(std::string service_type
            , std::string config
            , bool unique
            , int32_t workerid
            , uint32_t creatorid
            , int32_t sessionid);

        void remove_service(uint32_t serviceid, uint32_t sender, int32_t sessionid);

        void runcmd(uint32_t sender, const std::string& cmd, int32_t sessionid);

        void send_message(message_ptr_t&& msg) const;

        void send(uint32_t sender
            , uint32_t receiver
            , buffer_ptr_t buf
            , string_view_t header
            , int32_t sessionid
            , uint8_t type) const;

        void broadcast(uint32_t sender, const buffer_ptr_t& buf, string_view_t header, uint8_t type, bool only_unique);

        bool register_service(const std::string& type, register_func func);

        service_ptr_t make_service(const std::string& type);

        std::string get_env(const std::string& name) const;

        void set_env(std::string name, std::string value);

        uint32_t get_unique_service(const std::string& name) const;

        bool set_unique_service(std::string name, uint32_t v);

        size_t unique_service_size() const;

        log* logger() const;

        void response(uint32_t to
            , string_view_t header
            , string_view_t content
            , int32_t sessionid
            , uint8_t mtype = PTYPE_TEXT) const;

        uint32_t worker_id(uint32_t serviceid) const
        {
            return ((serviceid >> WORKER_ID_SHIFT) & 0xFF);
        }

        worker* get_worker(uint32_t workerid) const
        {
            --workerid;
            return workers_[workerid].get();
        }
    private:
        void set_server(server* sv);

        bool workerid_valid(uint32_t workerid) const
        {
            return (workerid > 0 && workerid <= static_cast<uint32_t>(workers_.size()));
        }

        worker* next_worker();
    private:
        std::atomic<uint32_t> next_workerid_;
        std::vector<std::unique_ptr<worker>>& workers_;
        std::unordered_map<std::string, register_func > regservices_;
        concurrent_map<std::string, std::string, rwlock> env_;
        concurrent_map<std::string, uint32_t, rwlock> unique_services_;
        log* logger_;
        server* server_;
    };
}