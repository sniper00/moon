#pragma once
#include <memory>

namespace moon
{
	enum class LogLevel
	{
		Error,
		Warn,
		Info,
		Debug,
		Trace,
		All
	};

	class Log
	{
	public:
		
		~Log();

		Log(const Log&) = delete;

		Log& operator=(const Log&) = delete;

		static Log& Instance();

		static void LogV(bool console, LogLevel level,  const char* fmt, ...);
	private:
		Log();
		static void Create(std::shared_ptr<Log>& v);
	private:
		struct Imp;
		std::shared_ptr<Imp>  m_Imp;
 	};

#define CONSOLE_TRACE(fmt, ...) moon::Log::LogV(true,moon::LogLevel::Trace,fmt,##__VA_ARGS__);
#define CONSOLE_DEBUG(fmt,...) moon::Log::LogV(true,moon::LogLevel::Debug,fmt,##__VA_ARGS__);  
#define CONSOLE_INFO(fmt,...) moon::Log::LogV(true,moon::LogLevel::Info,fmt,##__VA_ARGS__);  
#define CONSOLE_WARN(fmt,...) moon::Log::LogV(true,moon::LogLevel::Warn,fmt,##__VA_ARGS__);  
#define CONSOLE_ERROR(fmt,...) moon::Log::LogV(true,moon::LogLevel::Error,fmt,##__VA_ARGS__);  

#define LOG_TRACE(fmt, ...) moon::Log::LogV(false,moon::LogLevel::Trace,fmt,##__VA_ARGS__);
#define LOG_DEBUG(fmt,...) moon::Log::LogV(false,moon::LogLevel::Debug,fmt,##__VA_ARGS__);  
#define LOG_INFO(fmt,...) moon::Log::LogV(false,moon::LogLevel::Info,fmt,##__VA_ARGS__);  
#define LOG_WARN(fmt,...) moon::Log::LogV(false,moon::LogLevel::Warn,fmt,##__VA_ARGS__);  
#define LOG_ERROR(fmt,...) moon::Log::LogV(false,moon::LogLevel::Error,fmt,##__VA_ARGS__);

}


