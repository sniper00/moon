#include "log.h"
#include "common/sync_queue.hpp"
#define LOG_FILE_NAME "MoonNet.log"
#define MAX_LOG_LEN  8*1024
#define NEW_LINE "\n"

namespace moon
{
	static const char** level_string()
	{
		static const char*LogLevelString[(unsigned int)LogLevel::All] = { "Error","Warn","Info","Debug","Trace" };
		return LogLevelString;
	}

	static std::once_flag & once_flag()
	{
		static std::once_flag flag;
		return flag;
	}

	static time_t second()
	{
		return  std::time(nullptr);
	}

	static std::string format(LogLevel level, const char* tag, const char* info)
	{
		auto t = second();
		char timebuf[50] = { 0 };

#if TARGET_PLATFORM == PLATFORM_WINDOWS
		tm m;
		localtime_s(&m, &t);
		strftime(timebuf, array_szie(timebuf), "%Y/%m/%d %H:%M:%S", &m);
#else
		strftime(timebuf, array_szie(timebuf), "%Y/%m/%d %H:%M:%S", localtime(&t));
#endif

		std::stringstream ss;
		ss << timebuf << ' ';
		ss << std::setiosflags(std::ios::left) << std::setw(8) << std::this_thread::get_id();
		if (strlen(tag) != 0)
		{
			ss << std::setw(6) << level_string()[(int)level] << std::setw(10) << tag;
		}
		else
		{
			ss << std::setw(6) << level_string()[(int)level];
		}

		ss << info;
		return ss.str();
	}

	struct LogContext
	{
		LogLevel		Level;
		std::string		Content;
		bool				bConsole;
	};

	struct log::log_imp
	{
		sync_queue<LogContext, true>		LogQueue;
		std::thread										Thread;
		std::ofstream									LogFile;
		std::atomic_bool								bExit;

		log_imp()
			:bExit(false)
		{
		}

		~log_imp()
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
			Thread = std::thread(std::bind(&log_imp::write, this));
		}

		void wait()
		{
			while(LogQueue.size() > 0)
			{
				thread_sleep(100);
			}
			bExit = true;
		}

		void write()
		{
			if (!LogFile.is_open())
				return;

			do
			{
				auto ConsoleData = LogQueue.move();
				for (auto& it : ConsoleData)
				{
					if (it.bConsole)
					{
						std::cout << it.Content << std::endl;
					}

					LogFile.write(it.Content.data(), it.Content.size());
					if (it.Content.back() != '\n')
					{
						LogFile.write(NEW_LINE, strlen(NEW_LINE));
					}

					LogFile.flush();
				}
			} while (!bExit);
		}
	};

	log::log()
		:imp_(new log::log_imp)
	{
		imp_->Init();
	}

	log::~log()
	{
		imp_->bExit = true;
		imp_->LogQueue.exit();

		if (nullptr != imp_)
			delete imp_;
	}

	log& log::instance()
	{
		static std::shared_ptr<log> tmp;
		if (tmp == nullptr)
		{
			std::call_once(once_flag(), []() {
				assert(tmp == nullptr);
				auto p = new log();
				tmp = std::shared_ptr<log>(p);
			});
		}
		return *tmp;
	}

	void log::logfmt(bool console, LogLevel level, const char* tag, const char * fmt, ...)
	{
		if (!fmt) return;

		char fmtbuf[MAX_LOG_LEN] = { 0 };
		va_list ap;
		va_start(ap, fmt);
#if TARGET_PLATFORM == PLATFORM_WINDOWS
		vsnprintf_s(fmtbuf, MAX_LOG_LEN, fmt, ap);
#else
		vsnprintf(fmtbuf, MAX_LOG_LEN, fmt, ap);
#endif
		va_end(ap);

		auto str = format(level, tag, fmtbuf);
		LogContext   ctx;
		ctx.bConsole = console;
		ctx.Content = std::move(str);
		ctx.Level = level;
		instance().imp_->LogQueue.push_back(ctx);
	}

	void log::logstring(bool console, LogLevel level, const char* tag, const char* info)
	{
		auto str = format(level, tag, info);
		LogContext   ctx;
		ctx.bConsole = console;
		ctx.Content = std::move(str);
		ctx.Level = level;
		instance().imp_->LogQueue.push_back(ctx);
	}

	void log::wait()
	{
		instance().imp_->wait();
	}
}

