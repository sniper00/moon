/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include "config.h"

namespace moon
{
	DECLARE_SHARED_PTR(service);

	class MOON_EXPORT  service_pool final
	{
	public:
		static service_pool* instance();

		service_pool();

		~service_pool();

		service_pool(const service_pool&) = delete;

		service_pool& operator=(const service_pool&) = delete;

		service_pool(service_pool&&) = delete;

		void init(uint8_t machineid,uint8_t worker_num);

		uint8_t machineid();

		void run();

		void stop();

		void new_service(const std::string& type,const std::string& config, bool exclusived);

		void remove_service(uint32_t serviceid);

		void send(const message_ptr_t& msg);

		void broadcast(uint32_t sender, const message_ptr_t& msg);

		using register_func = service_ptr_t(*)();
		bool register_service(const std::string& type, register_func func);

		void report_service_num(uint32_t subscriber, const std::string& subject);

		void report_work_time(uint32_t subscriber, const std::string& subject);

	private:
		struct service_pool_imp;
		service_pool_imp*   imp_;
		static service_pool* instance_;
	};
};


