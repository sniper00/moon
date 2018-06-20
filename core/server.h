/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include "config.h"

namespace moon
{
    class log;
    class MOON_EXPORT  server final
    {
    public:
        using register_func = service_ptr_t(*)();

        server();

        ~server();

        server(const server&) = delete;

        server& operator=(const server&) = delete;

        server(server&&) = delete;

        void init(uint8_t worker_num, const std::string& logpath);

        void run();

        void stop();

        uint8_t workernum();

        size_t servicenum();

        uint32_t new_service(const std::string& service_type, bool unique, bool shareth,int workerid, const string_view_t& config);

        void runcmd(uint32_t sender, const buffer_ptr_t& buf, const std::string& header, int32_t responseid);

        void send_message(const message_ptr_t& msg) const;

        void send(uint32_t sender, uint32_t receiver, const buffer_ptr_t& buf,const string_view_t& header, int32_t responseid, uint8_t mtype) const;

        void broadcast(uint32_t sender, const message_ptr_t& msg);

        bool register_service(const std::string& type, register_func func);

        std::string get_env(const std::string& name);

        void set_env(const string_view_t& name, const string_view_t& value);

        uint32_t get_unique_service(const string_view_t& name);

        void set_unique_service(const string_view_t& name, uint32_t v);

        log* logger() const;

        void make_response(uint32_t sender, const string_view_t&, const string_view_t& content, int32_t resp, uint8_t mtype = PTYPE_TEXT) const;
    private:
        struct server_imp;
        server_imp*   imp_;
    };
};


