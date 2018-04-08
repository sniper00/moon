#pragma once
#include "component.h"

namespace moon
{
    enum class LogLevel
    {
        Error = 1,
        Warn,
        Info,
        Debug,
        Max
    };

    class MOON_EXPORT log
    {
    public:
        log();
        ~log();

        log(const log&) = delete;

        log& operator=(const log&) = delete;

        void init(const std::string& logfile);

        void logfmt(bool console, LogLevel level,const char* fmt, ...);

        void logstring(bool console, LogLevel level,string_view_t s);

        void set_level(LogLevel level);

        void set_level(string_view_t s);
    
        void wait();
    private:
        struct  log_imp;
        log_imp* imp_;
     };

#define CONSOLE_INFO(logger,fmt,...) logger->logfmt(true,moon::LogLevel::Info,fmt,##__VA_ARGS__);  
#define CONSOLE_WARN(logger,fmt,...) logger->logfmt(true,moon::LogLevel::Warn,fmt,##__VA_ARGS__);  
#define CONSOLE_ERROR(logger,fmt,...) logger->logfmt(true,moon::LogLevel::Error,fmt,##__VA_ARGS__);  

#define LOG_INFO(logger,fmt,...) logger->logfmt(false,moon::LogLevel::Info,fmt,##__VA_ARGS__);  
#define LOG_WARN(logger,fmt,...) logger->logfmt(false,moon::LogLevel::Warn,fmt,##__VA_ARGS__);  
#define LOG_ERROR(logger,fmt,...) logger->logfmt(false,moon::LogLevel::Error,fmt,##__VA_ARGS__);

#define CONSOLE_DEBUG(logger,fmt,...) logger->logfmt(true,moon::LogLevel::Debug,fmt,##__VA_ARGS__);  
#define LOG_DEBUG(logger,fmt,...) logger->logfmt(false,moon::LogLevel::Debug,fmt,##__VA_ARGS__); 
}


