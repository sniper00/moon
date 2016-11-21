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

	class XNET_DLL  service_pool
	{
	public:
		service_pool();
		~service_pool();
		service_pool(const service_pool&) = delete;
		service_pool& operator=(const service_pool&) = delete;
		service_pool(service_pool&&) = delete;

		void init(const std::string& config);

		uint8_t machineid();

		void run();

		void stop();

		void new_service(const service_ptr_t& s,const std::string& config, bool exclusived);

		void remove_service(uint32_t serviceid);

		void send(const message_ptr_t& msg);

		void broadcast(uint32_t sender, const std::string& data);
		void broadcast_ex(uint32_t sender, const buffer_ptr_t& data);
	private:
		struct service_pool_imp;
		service_pool_imp*   imp_;
	};
};


