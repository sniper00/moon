#pragma once
#include "asio.hpp"
#include "components/tcp/network.h"

namespace moon
{
	DECLARE_SHARED_PTR(session)
	DECLARE_SHARED_PTR(buffer)
	
	constexpr size_t IO_WORKER_ID_SHIFT = 16;
	constexpr size_t MAX_SESSION_UID = 0xFFFF;

	class io_worker
	{
	public:
		friend class session;

		io_worker();

		void run();

		void stop();

		asio::io_service& io_service();

		uint8_t workerid();

		void workerid(uint8_t v);

		session_ptr_t create_session(uint32_t time_out);

		void add_session(const session_ptr_t& session);

		void remove_session(uint32_t sessionid);

		void close_session(uint32_t sessionid);

		void send(uint32_t sessionid, const buffer_ptr_t& data);

		void	set_network_handle(const network_handle_t& handle);

		template<typename... Args>
		void operator()(Args&&... args)
		{
			handle_(std::forward<Args>(args)...);
		}

		int get_session_num();

	private:
		uint32_t make_sessionid();

		void start_checker();

		void	handle_checker(const asio::error_code &e);

		void remove(uint32_t sessionid);

		void close_all_sessions();
	private:
		asio::io_service ios_;
		asio::io_service::work work_;
		asio::steady_timer checker_;
		uint8_t workerid_;
		std::atomic<uint16_t> sessionuid_;
		std::atomic<int> sessionnum_;
		std::unordered_map<uint32_t, session_ptr_t> sessions_;
		network_handle_t handle_;
	};
}

