#pragma once
#include "common/macro_define.hpp"
#include "common/concurrent_queue.hpp"
#include "common/time.hpp"
#include "common/termcolor.hpp"
#include "common/path.hpp"
#include "common/object_pool.hpp"
#include "common/buffer.hpp"
#include "common/rwlock.hpp"

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

    class log
    {
        static const int MAX_LOG_LEN = 8 * 1024;
    public:
        log()
            :state_(state::init)
            , level_(LogLevel::Debug)
			, thread_(&log::write,this)
        {
        }

        ~log()
        {
            wait();
        }

        log(const log&) = delete;

        log& operator=(const log&) = delete;

        void init(const std::string& logfile)
        {
            if (ofs_)
            {
                ofs_->flush();
                ofs_->close();
            }

            if (!logfile.empty())
            {
                auto dir = path::directory(logfile);
                if (dir.size() != 0)
                {
                    if (!path::exist(dir))
                    {
                        MOON_CHECK(path::create_directory(dir),"create log dir failed");
                    }
                }
                ofs_.reset(new std::ofstream());
                ofs_->open(logfile, std::ofstream::out | std::ofstream::trunc);
            }
            state_.store(state::ready, std::memory_order_release);
        }

        void logfmt(bool console, LogLevel level, const char* fmt, ...)
        {
            if (level_ < level)
            {
                return;
            }

            if (nullptr == fmt)
                return;

            char fmtbuf[MAX_LOG_LEN];
            va_list ap;
            va_start(ap, fmt);
#if TARGET_PLATFORM == PLATFORM_WINDOWS
            int n = vsnprintf_s(fmtbuf, MAX_LOG_LEN, fmt, ap);
#else
            int n = vsnprintf(fmtbuf, MAX_LOG_LEN, fmt, ap);
#endif
            va_end(ap);
            logstring(console, level, moon::string_view_t(fmtbuf, n));
        }

        void logstring(bool console, LogLevel level, moon::string_view_t s)
        {
            if (level_ < level)
            {
                return;
            }

            auto ctx = buffer_cache_.create(console, level);
            moon::buffer* buf = &ctx->content_;
            format_header(buf, level);
            buf->write_back(s.data(), 0, s.size());
            buf->write_back("\n", 0, 1);
            log_queue_.push_back(ctx);
        }

        void set_level(LogLevel level)
        {
            level_ = level;
        }

        void set_level(string_view_t s)
        {
            if (moon::iequal_string(s, string_view_t{ "DEBUG" }))
            {
                set_level(LogLevel::Debug);
            }
            else  if (moon::iequal_string(s, string_view_t{ "INFO" }))
            {
                set_level(LogLevel::Info);
            }
            else  if (moon::iequal_string(s, string_view_t{ "WARN" }))
            {
                set_level(LogLevel::Warn);
            }
            else  if (moon::iequal_string(s, string_view_t{ "ERROR" }))
            {
                set_level(LogLevel::Error);
            }
            else
            {
                set_level(LogLevel::Debug);
            }
        }
    
        void wait()
        {
			if (state_.load() == state::exited)
			{
				return;
			}

			state_.store(state::exited);
			log_queue_.exit();

			if (thread_.joinable())
				thread_.join();

			if (ofs_&&ofs_->is_open())
			{
				ofs_->flush();
				ofs_->close();
				ofs_ = nullptr;
			}
        }
    private:
        void format_header(moon::buffer* buf, LogLevel level) const
        {
            size_t len = time::milltimestamp(buf->data(), 23);
            buf->offset_writepos(static_cast<int>(len));
            buf->write_back(" | ", 0, 3);
            len = moon::uint64_to_str(moon::thread_id(), buf->data() + buf->size());
            buf->offset_writepos(static_cast<int>(len));
            while (len < 6)
            {
                buf->write_back(" ", 0, 1);
                len++;
            }
            buf->write_back(to_string(level), 0, 11);
        }

        void write()
        {
            while (state_.load(std::memory_order_acquire) == state::init)
                std::this_thread::sleep_for(std::chrono::microseconds(50));

            std::vector<std::shared_ptr<log_line>> swaps;
            while (state_.load() == state::ready || log_queue_.size()!=0)
            {
                swaps.clear();
                log_queue_.swap(swaps);
                for (auto& it : swaps)
                {
                    if (it->bconsole_)
                    {
                        auto s = moon::string_view_t(reinterpret_cast<const char*>(it->content_.data()), it->content_.size());
                        switch (it->level_)
                        {
                        case LogLevel::Error:
                            std::cerr << termcolor::red << s;
                            break;
                        case LogLevel::Warn:
                            std::cout << termcolor::yellow << s;
                            break;
                        case LogLevel::Info:
                            std::cout << termcolor::white << s;
                            break;
                        case LogLevel::Debug:
                            std::cout << termcolor::green << s;
                            break;
                        default:
                            break;
                        }
                        std::cout << termcolor::white;
                    }

                    if (ofs_)
                    {
                        ofs_->write(it->content_.data(), it->content_.size());
                    }
                }
				if (ofs_)
				{
					ofs_->flush();
				}
            }
        }

        const char* to_string(LogLevel lv) const
        {
            switch (lv)
            {
            case LogLevel::Error:
                return " | ERROR | ";
            case LogLevel::Warn:
                return " | WARN  | ";
            case LogLevel::Info:
                return " | INFO  | ";
            case LogLevel::Debug:
                return " | DEBUG | ";
            default:
                return " | NULL  | ";
            }
        }

        struct log_line
        {
            log_line(bool bconsole, LogLevel level, size_t bufsize = 256)
                :bconsole_(bconsole)
                ,level_(level)
                ,content_(bufsize)
            {
            }

            void init(bool bconsole, LogLevel level)
            {
                bconsole_ = bconsole;
                level_ = level;
                content_.clear();
            }

            bool bconsole_;
            LogLevel level_;
            buffer content_;
        };

        std::atomic<state> state_;
        std::atomic<LogLevel> level_;
        std::unique_ptr<std::ofstream > ofs_;
        std::thread thread_;
        shared_pointer_pool<log_line, 1000, rwlock> buffer_cache_;
	concurrent_queue<std::shared_ptr<log_line>, std::mutex, true> log_queue_;
     };

#define CONSOLE_INFO(logger,fmt,...) logger->logfmt(true,moon::LogLevel::Info,fmt,##__VA_ARGS__);
#define CONSOLE_WARN(logger,fmt,...) logger->logfmt(true,moon::LogLevel::Warn,fmt" (%s:%d)",##__VA_ARGS__,__FILENAME__,__LINE__);
#define CONSOLE_ERROR(logger,fmt,...) logger->logfmt(true,moon::LogLevel::Error,fmt" (%s:%d)",##__VA_ARGS__,__FILENAME__,__LINE__);

#define LOG_INFO(logger,fmt,...) logger->logfmt(false,moon::LogLevel::Info,fmt,##__VA_ARGS__);  
#define LOG_WARN(logger,fmt,...) logger->logfmt(false,moon::LogLevel::Warn,fmt" (%s:%d)",##__VA_ARGS__,__FILENAME__,__LINE__);
#define LOG_ERROR(logger,fmt,...) logger->logfmt(false,moon::LogLevel::Error,fmt" (%s:%d)",##__VA_ARGS__,__FILENAME__,__LINE__);

#define CONSOLE_DEBUG(logger,fmt,...) logger->logfmt(true,moon::LogLevel::Debug,fmt" (%s:%d)",##__VA_ARGS__,__FILENAME__,__LINE__);
#define LOG_DEBUG(logger,fmt,...) logger->logfmt(false,moon::LogLevel::Debug,fmt" (%s:%d)",##__VA_ARGS__,__FILENAME__,__LINE__);
}


