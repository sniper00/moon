#pragma once
#include "config.h"

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

	class MOON_EXPORT log
	{
	public:
		~log();

		log(const log&) = delete;

		log& operator=(const log&) = delete;

		static log& instance();

		static void logfmt(bool console, LogLevel level, const char* tag, const char * fmt, ...);

		static void logstring(bool console, LogLevel level, const char* tag, const char* info);
	
		static void wait();
	private:
		log();
		struct  log_imp;
		log_imp* imp_;
 	};

#define CONSOLE_TRACE(fmt, ...) moon::log::logfmt(true,moon::LogLevel::Trace,"native",fmt,##__VA_ARGS__);
#define CONSOLE_DEBUG(fmt,...) moon::log::logfmt(true,moon::LogLevel::Debug,"native",fmt,##__VA_ARGS__);  
#define CONSOLE_INFO(fmt,...) moon::log::logfmt(true,moon::LogLevel::Info,"native",fmt,##__VA_ARGS__);  
#define CONSOLE_WARN(fmt,...) moon::log::logfmt(true,moon::LogLevel::Warn,"native",fmt,##__VA_ARGS__);  
#define CONSOLE_ERROR(fmt,...) moon::log::logfmt(true,moon::LogLevel::Error,"native",fmt,##__VA_ARGS__);  

#define LOG_TRACE(fmt, ...) moon::log::logfmt(false,moon::LogLevel::Trace,"native",fmt,##__VA_ARGS__);
#define LOG_DEBUG(fmt,...) moon::log::logfmt(false,moon::LogLevel::Debug,"native",fmt,##__VA_ARGS__);  
#define LOG_INFO(fmt,...) moon::log::logfmt(false,moon::LogLevel::Info,"native",fmt,##__VA_ARGS__);  
#define LOG_WARN(fmt,...) moon::log::logfmt(false,moon::LogLevel::Warn,"native",fmt,##__VA_ARGS__);  
#define LOG_ERROR(fmt,...) moon::log::logfmt(false,moon::LogLevel::Error,"native",fmt,##__VA_ARGS__);

#define CONSOLE_TRACE_TAG(tag,fmt, ...) moon::log::logfmt(true,moon::LogLevel::Trace,tag,fmt,##__VA_ARGS__);
#define CONSOLE_DEBUG_TAG(tag,fmt,...) moon::log::logfmt(true,moon::LogLevel::Debug,tag,fmt,##__VA_ARGS__);  
#define CONSOLE_INFO_TAG(tag,fmt,...) moon::log::logfmt(true,moon::LogLevel::Info,tag,fmt,##__VA_ARGS__);  
#define CONSOLE_WARN_TAG(tag,fmt,...) moon::log::logfmt(true,moon::LogLevel::Warn,tag,fmt,##__VA_ARGS__);  
#define CONSOLE_ERROR_TAG(tag,fmt,...) moon::log::logfmt(true,moon::LogLevel::Error,tag,fmt,##__VA_ARGS__);  

#define LOG_TRACE_TAG(tag,fmt, ...) moon::log::logfmt(false,moon::LogLevel::Trace,tag,fmt,##__VA_ARGS__);
#define LOG_DEBUG_TAG(tag,fmt,...) moon::log::logfmt(false,moon::LogLevel::Debug,tag,fmt,##__VA_ARGS__);  
#define LOG_INFO_TAG(tag,fmt,...) moon::log::logfmt(false,moon::LogLevel::Info,tag,fmt,##__VA_ARGS__);  
#define LOG_WARN_TAG(tag,fmt,...) moon::log::logfmt(false,moon::LogLevel::Warn,tag,fmt,##__VA_ARGS__);  
#define LOG_ERROR_TAG(tag,fmt,...) moon::log::logfmt(false,moon::LogLevel::Error,tag,fmt,##__VA_ARGS__);

}


