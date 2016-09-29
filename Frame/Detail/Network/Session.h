/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include "asio.hpp"
#include "NetworkDefine.h"


namespace moon
{
	DECLARE_SHARED_PTR(Session);

	constexpr int32_t			IO_BUFFER_SIZE = 8192;

	//asio::socket 的封装
	class NetworkService;
	class Session :public std::enable_shared_from_this<Session>,private asio::noncopyable
	{	
	public:

		Session(NetMessageDelegate& netDelegate,NetworkService& serv);

		~Session(void);

		/**
		* 连接成功后调用此函数，可以做一些初始化工作
		*
		* @return ,if return true will add to NetworkService else will close
		*/
		bool											Start();

		/**
		* 强制关闭该socket连接
		*
		* @state 设置一个关闭状态
		*/
		void											Close(ESocketState state);

		/**
		* 向该socket连接发送数据
		*
		*/
		void											Send(const MemoryStreamPtr& data);

		/**
		* 获取asio::ip::tcp::socket
		*
		*/
		asio::ip::tcp::socket&				GetSocket() { return m_Socket; };

		/**
		* 获取错误信息
		*
		*/
		std::string									GetErrorMessage() { return m_ErrorCode.message(); }

		/**
		* 检测这个网络连接的状态
		*
		*/
		bool											IsOk();
	private:
		/**
		* 投递异步读请求
		*
		*/
		void											PostRead();

		/**
		* 投递异步写请求
		*
		*/
		void											PostSend();

		/**
		* 读取完成回掉
		*
		*/
		void											HandleRead(const asio::error_code& e, std::size_t bytes_transferred);

		/**
		* 写完成回掉
		*
		*/
		void											HandleSend(const asio::error_code& e, std::size_t bytes_transferred);
	
		/**
		* 刷新最后接收到到数据的时间
		*
		*/
		void											RefreshLastRecevieTime();

		/**
		* 连接成功,发送关闭消息给模块
		*
		*/
		void											OnConnect();

		/**
		* 接收到消息,发送关闭消息给模块
		*
		*/
		void											OnMessage(const uint8_t* data,size_t len);

		/**
		* 连接将要关闭,发送关闭消息给模块
		*
		*/
		void											OnClose();

		/**
		* 解析远程的地址
		*
		*/
		void											ParseRemoteEndPoint();


		ESocketState							GetState() { return m_State; }
	public:
		//SessionID high 8 bit is NetworkService id.
		PROPERTY_READWRITE(SessionID, m_ID, ID)
		PROPERTY_READONLY(int64_t, m_LastRecevieTime, LastRecevieTime)
		PROPERTY_READONLY(std::string, m_RemoteIP, RemoteIP)
		PROPERTY_READONLY(uint16_t, m_RemotePort, RemotePort)
	private:
		//消息处理函数对象
		NetMessageDelegate&			m_Delegate;

		NetworkService&						m_Service;

		asio::ip::tcp::socket					m_Socket;
		//异步接收缓冲区
		uint8_t										m_RecvBuffer[IO_BUFFER_SIZE];
		//接收消息缓冲区
		MemoryStream						m_RecvMemoryStream;
		//发送缓冲区
		MemoryStream						m_SendMemoryStream;
		//发送消息发送队列
		std::deque<MemoryStreamPtr>	m_SendQueue;
		//是否正在发送
		bool											m_IsSending;
		//网络错误
		asio::error_code						m_ErrorCode;

		ESocketState							 m_State;
	};
};





