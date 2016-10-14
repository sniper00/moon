#pragma once
#include "MacroDefine.h"

namespace moon
{
	class Message;

	class Network
	{
	public:
		Network();

		~Network();

		/**
		* 初始化网络
		*
		* @threadNum 网络线程数
		*/
		void				InitNet(int threadNum);

		/**
		* 网络监听的地址
		*
		* @ip
		* @port
		*/
		bool				Listen(const std::string& ip, const std::string& port);

		/**
		* 异步连接服务器（可连接多个）
		*
		* @ip
		* @port
		*/
		void				Connect(const std::string& ip, const std::string& port);

		/**
		* 同步连接服务器
		*
		* @ip
		* @port
		* @return 服务器连接id
		*/
		SessionID		SyncConnect(const std::string& ip, const std::string& port);

		/**
		* 向某个链接发送消息
		*
		* @sessionID
		*/
		void				Send(SessionID sessionID,const std::string& data);

		/**
		* 强制关闭一个网络连接
		*
		* @sessionID
		*/
		void				Close(SessionID sessionID);

		/**
		* 设置Session的超时检测
		* @timeout 超时时间 ，单位 s
		*/
		void				SetTimeout(uint32_t timeout);

		/**
		* 网络消息处理回掉
		*/
		void				SetHandler(const std::function<void(uint32_t, const std::string&, uint8_t)>&);
	public:
		void				Start();

		void				Update(uint32_t interval);

		void				Destory();
	private:
		struct  NetworkImp;
		std::shared_ptr<NetworkImp>											m_NetworkImp;
	};
}



