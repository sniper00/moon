#pragma once
#include "config.h"
#include "common/concurrent_map.hpp"
#include "common/rwlock.hpp"
#include "common/log.hpp"

namespace asio {
    class io_context;
}

namespace moon
{
    using env_t = concurrent_map<std::string, std::string, rwlock>;
    using unique_service_db_t = concurrent_map<std::string, uint32_t, rwlock>;

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

        size_t servicenum() const;

        size_t workernum() const;

        uint32_t new_service(const std::string& service_type,string_view_t config, bool unique, int32_t workerid, uint32_t creatorid, int32_t responseid);

        void remove_service(uint32_t serviceid, uint32_t sender, int32_t respid, bool crashed = false);

        void runcmd(uint32_t sender, const std::string& cmd, int32_t responseid);

        void send_message(message_ptr_t&& msg) const;

        void send(uint32_t sender, uint32_t receiver, const buffer_ptr_t& buf, string_view_t header, int32_t responseid, uint8_t type) const;

        void broadcast(uint32_t sender, const buffer_ptr_t& buf, string_view_t header, uint8_t type);

        bool register_service(const std::string& type, register_func func);

        std::string get_env(const std::string& name) const;

        void set_env(std::string name, std::string value);

        uint32_t get_unique_service(const std::string& name) const;

        void set_unique_service(std::string name, uint32_t v);

        log* logger() const;

        void response(uint32_t to, string_view_t header, string_view_t content, int32_t responseid, uint8_t mtype = PTYPE_TEXT) const;

        void on_service_remove(uint32_t serviceid);

        asio::io_context& get_io_context(uint32_t serviceid);
    private:
        void set_server(server* sv);

        uint32_t worker_id(uint32_t serviceid) const
        {
            return ((serviceid >> WORKER_ID_SHIFT) & 0xFF);
        }

        bool workerid_valid(uint32_t workerid) const
        {
            return (workerid > 0 && workerid <= static_cast<int32_t>(workers_.size()));
        }

        worker* get_worker(uint32_t workerid) const
        {
            --workerid;
            return workers_[workerid].get();
        }

        worker* next_worker();

        bool has_serviceid(uint32_t serviceid) const;

        bool try_add_serviceid(uint32_t serviceid);
    private:
        std::atomic<uint32_t> next_workerid_;
        std::vector<std::unique_ptr<worker>>& workers_;
        std::unordered_map<std::string, register_func > regservices_;
        mutable rwlock serviceids_lck_;
        std::unordered_set<uint32_t> serviceids_;
        env_t env_;
        unique_service_db_t unique_services_;
        log* logger_;
        server* server_;
    };
}