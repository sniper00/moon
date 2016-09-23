#pragma once
#include "MacroDefine.h"

namespace moon
{
	DECLARE_SHARED_PTR(Message)

	class NetworkComponent
	{
	public:
		NetworkComponent();
		~NetworkComponent();

		/**
		* 初始化网络
		*
		* @thread_num 网络线程数
		*/
		void InitNet(int netThreadNum);

		/**
		* 网络监听的地址
		*
		* @ip
		* @port
		*/
		bool	Listen(const std::string& ip, const std::string& port);

		/**
		* 异步连接服务器（可连接多个）
		*
		* @ip
		* @port
		*/
		void	Connect(const std::string& ip, const std::string& port);

		SessionID SyncConnect(const std::string& ip, const std::string& port);

		void SendNetMessage(SessionID sessionID, const MessagePtr& msg);

		/**
		* 强制关闭一个网络连接
		*
		* @sessionID
		*/
		void	Close(SessionID sessionID);

		/**
		* 网络消息处理回掉
		*
		*/
		std::function<void(const MessagePtr&)> OnNetMessage;

	//protected:
		void OnEnter();

		void Update(uint32_t interval);

		void OnExit();
	private:
		struct  NetworkComponentImp;
		std::shared_ptr<NetworkComponentImp>		m_NetworkComponentImp;
	};
}



