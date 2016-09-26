/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include "NetworkService.h"

namespace moon
{
	DECLARE_SHARED_PTR(NetworkService);

	using  NetworkServiceMap = std::unordered_map<uint8_t, NetworkServicePtr>;

	class NetworkServicePool
	{
	public:
		NetworkServicePool(uint8_t pool_size);
		~NetworkServicePool(void);

		void Run();

		void Stop();

		void	SendMessage(SessionID sessionID, const MemoryStreamPtr& msg);

		void	CloseSession(SessionID sessionID, ESocketState state);

		NetworkService& PollAService();

		NetworkServiceMap& GetServices() { return m_Services; }

	private:
		NetworkServiceMap									m_Services;
		std::vector<std::shared_ptr<std::thread>>	m_Threads;

		// The next NetworkService to use for a connection.
		std::atomic<uint8_t>									m_NextService;
	};

}




