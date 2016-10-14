/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#include  "NetworkService.h"
#include  "Session.h"

using namespace moon;

NetworkService::NetworkService()
	:m_IoWork(m_IoService),m_Checker(m_IoService), m_TimeOut(0),m_IncreaseSessionID(0)
{

}

NetworkService::~NetworkService(void)
{

}

asio::io_service& NetworkService::GetIoService()
{
	return m_IoService;
}

void NetworkService::AddSession(const SessionPtr& sessionPtr)
{
	auto ID = ++m_IncreaseSessionID;
	if (ID == 0)
	{
		ID = ++m_IncreaseSessionID;
	}

	SessionID sessionID;
	sessionID = uint32_t(m_ID) << 24 | ID;
	sessionPtr->SetID(sessionID);

	m_IoService.post([this, sessionPtr]() {
		if (!sessionPtr->Start())
			return;

		auto Ret = m_Sessions.emplace(sessionPtr->GetID(), std::move(sessionPtr));
		if (!Ret.second)
		{
			assert(0);
			//SessionID repeated.
		}	
	});
}

void NetworkService::RemoveSession(SessionID sessionID)
{
	m_IoService.post([this, sessionID]() {
		if (m_Sessions.find(sessionID) != m_Sessions.end())
		{
			m_Sessions.erase(sessionID);
		}
	});
}

void moon::NetworkService::SetTimeout(uint32_t timeout)
{
	m_TimeOut = timeout;
}

void NetworkService::Send(SessionID sessionID, const MemoryStreamPtr& msg)
{
	m_IoService.post([this, sessionID, msg]()
	{
		auto iter = m_Sessions.find(sessionID);
		if (iter != m_Sessions.end())
		{
			iter->second->Send(msg);
		}
	});
}

void NetworkService::Run()
{
	if (m_TimeOut != 0)
	{
		m_IoService.post([this]() {
			m_Checker.expires_from_now(std::chrono::seconds(10));
			m_Checker.async_wait(std::bind(&NetworkService::TimeoutChecker, this, std::placeholders::_1));
		});
	}

	m_IoService.run();
}

void NetworkService::Stop()
{
	m_IoService.post([this]() {
		for (auto& iter : m_Sessions)
		{
			iter.second->Close(ESocketState::Ok);
		}
		m_Sessions.clear();
	});
	m_IoService.stop();
}

void NetworkService::CloseSession(SessionID sessionID, ESocketState state)
{
	m_IoService.post([this, sessionID, state]()
	{
		auto iter = m_Sessions.find(sessionID);
		if (iter != m_Sessions.end())
		{
			iter->second->Close(state);
		}
	});
}

void moon::NetworkService::TimeoutChecker(const asio::error_code &)
{
	m_Checker.expires_from_now(std::chrono::seconds(10));
	m_Checker.async_wait(std::bind(&NetworkService::TimeoutChecker, this, std::placeholders::_1));

	auto curSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

	for (auto& iter : m_Sessions)
	{
		int64_t diff = curSec - iter.second->GetLastRecevieTime();
		if (diff >= m_TimeOut)
		{
			//投递socket连接关闭请求，此操作是异步的
			iter.second->Close(ESocketState::Timeout);
		}
	}
}



