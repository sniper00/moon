#include "Log.h"
#include <cstdarg>
#include <iostream>
#include <chrono>
#include <ctime>
#include <sstream>
#include <fstream>
#include <cstring>

#include "Common/SyncQueue.hpp"
#include "Common/StringUtils.hpp"
#include "MacroDefine.h"

#define LOG_FILE_NAME "MoonNet.log"
#define MAX_LOG_LEN  32*1024

#if TARGET_PLATFORM == PLATFORM_WINDOWS
#define NEW_LINE "\r\n"
#else
#define NEW_LINE "\n"
#endif

namespace moon
{
	static std::once_flag flag;
	static const char* LogLevelString [(unsigned int)LogLevel::All] = {"Error\t","Warn\t","Info\t","Debug\t","Trace\t" };
	struct LogContext
	{
		LogLevel		Level;
		std::string		Content;
		bool				bConsole;
	};

	static time_t GetCurTime()
	{
		return  std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	}

	static std::string Format(LogLevel level, const char* fmt, va_list v)
	{
		auto t = GetCurTime();
		char timebuf[50] = { 0 };
	
#if TARGET_PLATFORM == PLATFORM_WINDOWS
		tm m;
		localtime_s(&m, &t);
		strftime(timebuf, array_szie(timebuf), "[%Y/%m/%d %H:%M:%S]", &m);
#else
		strftime(timebuf, array_szie(timebuf), "[%Y/%m/%d %H:%M:%S]", localtime(&t));
#endif
		
		std::stringstream ss;
		ss << timebuf;
		ss << '[' << std::this_thread::get_id();
		ss << "]\t";
		ss << LogLevelString[(int)level];
		char fmtbuf[MAX_LOG_LEN] = { 0 };
#if TARGET_PLATFORM == PLATFORM_WINDOWS
		vsnprintf_s(fmtbuf, MAX_LOG_LEN, fmt, v);
#else
		vsnprintf(fmtbuf, MAX_LOG_LEN, fmt, v);
#endif
		ss << fmtbuf;
		return ss.str();
	}

	void Log::Create(std::shared_ptr<Log>& v)
	{
		assert(v == nullptr);
		auto p = new Log();
		v = std::shared_ptr<Log>(p);
	}

	struct Log::Imp
	{
		SyncQueue<LogContext, 50, true>		LogQueue;
		std::thread										Thread;
		std::ofstream									LogFile;
		std::atomic_bool								bExit;

		Imp()
			:bExit(false)
		{
		}

		~Imp()
		{
			if (Thread.joinable())
			{
				Thread.join();
			}

			if (LogFile.is_open())
			{
				LogFile.flush();
				LogFile.close();
			}
		}

		void Init()
		{
			LogFile.open(LOG_FILE_NAME);
			Thread = std::thread(std::bind(&Imp::WriteLog, this));
		}

		void WriteLog()
		{
			if (!LogFile.is_open())
				return;

			while (!bExit)
			{
				auto ConsoleData = LogQueue.Move();
				for (auto& it : ConsoleData)
				{
					if (it.bConsole)
					{
						std::cout << it.Content << std::endl;
					}

					LogFile.write(it.Content.data(), it.Content.size());
					LogFile.write(NEW_LINE, strlen(NEW_LINE));

					LogFile.flush();
				}
			}

			auto ConsoleData = LogQueue.Move();
			for (auto& it : ConsoleData)
			{
				if (it.bConsole)
				{
					std::cout << it.Content << std::endl;
				}
				LogFile.write(it.Content.data(), it.Content.size());
				LogFile.write(NEW_LINE, strlen(NEW_LINE));
				LogFile.flush();
			}
		}
	};

	moon::Log& moon::Log::Instance()
	{
		static std::shared_ptr<Log> tmp;
		if (tmp == nullptr)
		{
			std::call_once(flag, []() {
				assert(tmp == nullptr);
				auto p = new Log();
				tmp = std::shared_ptr<Log>(p);
			});
		}
		return *tmp;
	}

	Log::Log()
		:m_Imp(std::make_shared<Log::Imp>())
	{
		m_Imp->Init();
	}

	Log::~Log()
	{
		m_Imp->bExit = true;
		m_Imp->LogQueue.Exit();
	}

	void Log::LogV(bool console, LogLevel level, const char * fmt, ...)
	{
		if (!fmt) return;
		va_list ap;
		va_start(ap, fmt);
		auto str = Format(level,fmt, ap);
		va_end(ap);

		LogContext   ctx;
		ctx.bConsole = console;
		ctx.Content = str;
		ctx.Level = level;
		Instance().m_Imp->LogQueue.PushBack(ctx);
	}
}


