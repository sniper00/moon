/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include "config.h"
#include "common/loop_thread.hpp"
#include "common/async_event.hpp"

#define MACHINE_ID_SHIFT 24
#define WORKER_ID_SHIFT 16

namespace moon
{
	class service_pool;
	DECLARE_SHARED_PTR(service)

	/*
	* Do not set async_event max queue size, it may dead wait.
	*/
	class service_worker:public loop_thread,public async_event
	{
	public:
		friend class service_pool;
		friend class service;

		service_worker();

		~service_worker();
	private:
		void run();

		void stop();

		uint32_t make_serviceid();

		void new_service(const service_ptr_t&s, const std::string& config, bool exclusived);

		void remove_service(uint32_t id);
   
		void send(const message_ptr_t& msg);
    
		int64_t	work_time();

		uint8_t	workerid();

		void	workerid(uint8_t id);

		void machineid(uint8_t v);

		void set_service_pool(service_pool* v);

		void	exclusive(bool v);

		bool	exclusive();

		void push_message(const message_ptr_t& msg);
	
		service* find(uint32_t serviceid);

		void	update(uint32_t interval);
	private:
		uint8_t	 workerid_;
		uint8_t  machineid_;
		uint16_t serviceuid_;
		uint32_t message_counter_;
		service_pool*  pool_;
		std::atomic_bool exclusived_;
		std::atomic<int64_t> work_time_;
		std::unordered_map<uint32_t, service_ptr_t> services_;
		std::deque<message_ptr_t> message_queue_;
	};
};


