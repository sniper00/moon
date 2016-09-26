/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include "NetworkDefine.h"

namespace moon
{
	/**
	* NetWorkFrame 实现了网络库操作接口。 NetWorkFrame 对象本身是非线程安全的。
	*/
	class NetWorkFrame
	{
	public:

		/**
		* NetWorkFrame 构造函数
		* @handler 网络事件回掉（connect, recevie, close)
		* @net_thread_num 网络线程数量
		* 注意：如果开启了多个网络线程，那么handler 回掉函数是非线程安全的。
		*/
		NetWorkFrame(const NetMessageDelegate& handler, uint8_t threadNum = 1);
		~NetWorkFrame();

		/**
		* 监听某个端口
		* @listenAddress ip地址或者域名
		* @listenPort 端口
		*/
		void							Listen(const std::string& ip, const std::string& port);

		/**
		* 异步连接某个端口
		* @ip ip地址或者域名
		* @port 端口
		*/
		void							AsyncConnect(const std::string& ip, const std::string& port);

		/**
		* 同步连接某个端口
		* @ip ip地址或者域名
		* @port 端口
		* @return 返回链接的 socketID, 成功 socketID.value != 0, 失败socketID.value = 0
		*/
		SessionID			SyncConnect(const std::string& ip, const std::string& port);

		/**
		* 向某个链接发送数据, 这个函数是线程安全的
		* @sessionID 连接标识
		* @data 数据
		*/
		void							Send(SessionID sessionID, const MemoryStreamPtr& msg);

		/**
		* 关闭一个链接
		* @sessionID 连接标识
		* @state 给链接设置一个状态，表明为什么关闭（例如 超时，发送非法数据）
		*/
		void							CloseSession(SessionID sessionID, ESocketState state);

		/**
		* 启动网络库 网络库运行在子线程，不会阻塞主线程
		*/
		void							Run();

		/**
		* 停止网络库
		*/
		void							Stop();

		/**
		* 获取错误码
		*/
		int							GetErrorCode();

		/**
		* 获取错误信息
		*/
		std::string					GetErrorMessage();

		/**
		* 设置管理链接的超时检测
		* @timeout 超时时间 ，单位 ms
		* @checkInterval 超时检测间隔，单位 ms
		*/
		void							SetTimeout(uint32_t timeout, uint32_t checkInterval);
	protected:
		/**
		* 投递异步accept,接受网络连接
		*/
		void							PostAccept();

		/**
		* 超时检测线程函数
		*/
		void							CheckTimeOut(uint32_t interval);
	protected:
		struct Imp;
		std::shared_ptr<Imp>  m_Imp;
		NetMessageDelegate   m_Delegate;
	};
};
