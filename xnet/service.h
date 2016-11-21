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
	DECLARE_SHARED_PTR(buffer);
	DECLARE_SHARED_PTR(service);

	class service_pool;

	class XNET_DLL service
	{
	public:
		friend class service_pool;
		friend class service_worker;

		service();

		virtual ~service();

		uint32_t serviceid() const;

		std::string name() const;
	
		void set_enbale_update(bool v);

		bool enable_update();

		void send(uint32_t receiver, const std::string& data, const std::string& userctx, uint32_t rpc, uint8_t type);

		void broadcast(const std::string& data);

		void broadcast_ex(const buffer_ptr_t& data);

		void send_cache(uint32_t receiver, uint32_t cacheid, const std::string& userctx, uint32_t rpc, uint8_t);

		uint32_t make_cache(const std::string & data);

		void new_service(const std::string & config);

		void remove_service(uint32_t id);

		void init_net(uint8_t threadnum, uint32_t timeout);

		void net_send(uint32_t sessionid, const buffer_ptr_t& data);

		void listen(const std::string&ip, const std::string& port);

		uint32_t sync_connect(const std::string&ip, const std::string& port);

		void async_connect(const std::string&ip, const std::string& port);

		std::string get_config(const std::string& key);

		void send_imp(uint32_t receiver, const buffer_ptr_t& ms, const std::string& userctx, uint32_t rpc, uint8_t);

	protected:
		virtual service_ptr_t clone() = 0;

		virtual bool init(const std::string& config);

		virtual void start();

		virtual void on_message(message* msg) {}

		virtual void destory();

		virtual void update();

		void set_direct_send(const std::function<void(const message_ptr_t& msg)>& f);

		void set_serviceid(uint32_t v);

		void set_name(const std::string& name);

		void set_service_pool(service_pool* pool);

		void exit(bool v);

		bool exit();

		void handle_message(const message_ptr_t& msg);
	protected:
		struct service_imp;
		service_imp* service_imp_;
	};
}

