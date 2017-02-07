/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include "config.h"
#include "component.h"

namespace moon
{
	class service_pool;
	class service_worker;

	class MOON_EXPORT service:public component
	{
	public:
		friend class service_pool;
		friend class service_worker;

		service();

		service(const service&) = delete;

		service& operator=(const service&) = delete;

		virtual ~service();

		uint32_t serviceid() const;

		void send(uint32_t receiver, const std::string& data, const std::string& userctx, uint32_t rpc, message_type type);

		void broadcast(const message_ptr_t& msg);

		void send_cache(uint32_t receiver, uint32_t cacheid, const std::string& userctx, uint32_t rpc, message_type );

		uint32_t make_cache(const std::string & data);

		void new_service(const std::string& type, const std::string& config);

		void remove_service(uint32_t id);

		void send_imp(uint32_t receiver, const buffer_ptr_t& data, const std::string& userctx, uint32_t rpc, message_type);

		void report_service_num(const std::string& subject);

		void report_work_time(const std::string& subject);

	protected:
		virtual bool init(const std::string& config) = 0;

		virtual void on_message(message* msg) = 0;

		virtual void update();

		void set_serviceid(uint32_t v);

		void set_service_worker(service_worker* w);

		void exit(bool v);

		bool exit();

		void handle_message(const message_ptr_t& msg);
	private:
		struct service_imp;
		service_imp* service_imp_;
		service_pool* pool_;
		service_worker* worker_;
	};
}

