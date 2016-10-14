/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once

#include "asio.hpp"
#include "asio/steady_timer.hpp"
#include "NetworkDefine.h"

namespace moon
{
	DECLARE_SHARED_PTR(Session);
	DECLARE_SHARED_PTR(memory_stream);
	//asio::io_services 的封装 ， 一条线程一个NetworkService
	class NetworkService
	{
	public:
		NetworkService();
		~NetworkService(void);

		void Run();

		void Stop();

		/**
		* 获取asio::io_services
		*
		* @return asio::io_services
		*/
		asio::io_service&	GetIoService();
		/**
		* 向这个NetworkService添加 Session
		*
		* @session 
		*/
		void			AddSession(const SessionPtr& session);

		/**
		* 移除Session
		*
		* @sessionID Session的唯一标识符
		*/
		void			RemoveSession(SessionID sessionID);

		/**
		* 设置超时间隔
		*
		* @timeout 超时间隔 s
		*/
		void			SetTimeout(uint32_t timeout);

		/**
		* 向某个socket连接 发送数据
		*
		* @socketID
		* @buffer_ptr 数据
		*/
		void			Send(SessionID sessionID, const MemoryStreamPtr& msg);

		/**
		* 关闭某个socket连接
		*
		* @sessionID 
		* @state 设置关闭状态
		*/
		void			CloseSession(SessionID sessionID, ESocketState why);

		/**
		* 获取serviceID
		*
		*/

		PROPERTY_READWRITE(uint32_t, m_ID, ID)
	private:
		/**
		* 超时检测
		*
		*/
		void			TimeoutChecker(const asio::error_code&);

	private:
		asio::io_service																m_IoService;
		asio::io_service::work													m_IoWork;
		asio::steady_timer														m_Checker;
		std::unordered_map<SessionID, SessionPtr>				m_Sessions;
		uint32_t																		m_TimeOut;
		uint32_t																		m_IncreaseSessionID;
	};
}




