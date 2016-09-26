/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#include  "NetworkServicePool.h"

using namespace moon;

NetworkServicePool::NetworkServicePool(uint8_t pool_size)
{
	m_NextService = 0;

	pool_size = pool_size > 0 ? pool_size : 1;

	for(uint8_t i = 0; i<pool_size ; ++i)
	{
		auto tmp = ObjectCreateHelper<NetworkService>::Create();
		tmp->SetID(i);
		m_Services.emplace(i, tmp);
	}
}


NetworkServicePool::~NetworkServicePool(void)
{

}

void NetworkServicePool::Run()
{
	m_Threads.clear();
	// Create a pool of threads to run all of the io_services.

	for (auto iter : m_Services)
	{
		std::shared_ptr<std::thread> thread(new std::thread(
			std::bind(&NetworkService::Run, iter.second)));
		m_Threads.push_back(thread);
	}
}

void NetworkServicePool::Stop()
{
	// Explicitly stop all io_services. 
	for (auto iter : m_Services)
	{
		iter.second->Stop();
	}

	for (auto iter : m_Threads)
	{
		if (iter->joinable())
		{
			iter->join();
		}
	}
}

void moon::NetworkServicePool::SendMessage(SessionID sessionID, const MemoryStreamPtr& msg)
{
	uint8_t servicesid = (sessionID >> 24)&0xFF;
	auto iter = m_Services.find(servicesid);
	if (iter != m_Services.end())
	{
		iter->second->SendMessage(sessionID, msg);
	}
}

void moon::NetworkServicePool::CloseSession(SessionID sessionID, ESocketState state)
{
	uint8_t servicesid = (sessionID >> 24) & 0xFF;
	auto iter = m_Services.find(servicesid);
	if (iter != m_Services.end())
	{
		iter->second->CloseSession(sessionID, state);
	}
}

NetworkService& NetworkServicePool::PollAService()
{
	// Use a round-robin scheme to choose the next io_service to use. 
	auto& ret = m_Services[m_NextService];
	++m_NextService;
	if (m_NextService == m_Services.size())
		m_NextService = 0;
	return *ret;
}


