/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include "config.h"
#include "component.h"

namespace moon
{
    class server;
    class worker;
    class log;

    class MOON_EXPORT service:public component
    {
    public:
        friend class server;
        friend class worker;

        service();

        virtual ~service();

        bool unique() const;

        uint32_t id() const;

        uint32_t make_cache(const buffer_ptr_t& buf);

        void send_cache(uint32_t receiver, uint32_t cacheid, const string_view_t& header, int32_t responseid, uint8_t mtype ) const;
    
        void handle_message(const message_ptr_t& msg);

        log* logger() const override;

        server* get_server() const;

        worker* get_worker() const;

        void removeself(bool crashed = false);
    protected:
        void set_unique(bool v);

        void set_id(uint32_t v);

        void set_worker(worker* w);

        virtual bool init(const std::string& config) = 0;

        virtual void dispatch(message* msg) = 0;

        virtual void exit();
    private:
        struct service_imp;
        service_imp* service_imp_;
    };
}

