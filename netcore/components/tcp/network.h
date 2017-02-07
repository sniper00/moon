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
	enum class network_logic_error
	{
		ok = 0,
		message_size_max = 1, // send or recv message size too long
		socket_read_timeout = 2, //socket read failed
	};

	constexpr message_size_t IO_BUFFER_SIZE = 8192;
	constexpr message_size_t MAX_NMSG_SIZE = 8192;

	using network_handle_t = std::function<void(const message_ptr_t&)>;

	class MOON_EXPORT network:public component
	{
	public:
		network();

		~network();

		void init(int nthread, uint32_t timeout);

		void set_max_queue_size(size_t size);

		void	set_network_handle(const network_handle_t& handle);

		void start() override;

		void destory() override;

		void update() override;

		void listen(const std::string& ip, const std::string& port);

		void async_connect(const std::string & ip, const std::string & port);

		uint32_t sync_connect(const std::string & ip, const std::string & port);

		void send(uint32_t sessionid,const buffer_ptr_t& data);

		void close(uint32_t sessionid);

		int get_session_num();

	private:
		void start_accept();

		bool ok();
	private:
		struct network_imp;
		network_imp* network_imp_;
	};
};
