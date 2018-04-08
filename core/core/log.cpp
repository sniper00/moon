#include "log.h"
#include "common/concurrent_queue.hpp"
#include "common/time.hpp"
#include "common/termcolor.hpp"
#include "common/path.hpp"
#include "common/object_pool.hpp"
#include "common/buffer.hpp"
#include "common/buffer_writer.hpp"
#include "common/rwlock.hpp"

#define MAX_LOG_LEN  8*1024

namespace moon
{
    DECLARE_SHARED_PTR(buffer);

    const char*level_string[static_cast<int>(LogLevel::Max)] = { " | NULL  | "," | ERROR | "," | WARN  | "," | INFO  | "," | DEBUG | "};

    struct log_context
    {
        log_context(bool bconsole, LogLevel level, size_t bufsize = 256)
            :bconsole_(bconsole)
            , level_(level)
            , content_(bufsize)
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

    void format_header(moon::buffer* buf, LogLevel level)
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
        buf->write_back(level_string[static_cast<int>(level)], 0, 11);
    }

    struct log::log_imp
    {
        std::atomic_bool bexit_;
        std::atomic<LogLevel> level_;
        FILE* log_file_;
        std::thread thread_;
        shared_pointer_pool<log_context, 1000, rwlock> buffer_cache_;
        sync_queue<std::shared_ptr<log_context>, std::mutex,true> log_queue_;
        
        log_imp()
            :bexit_(true)
            , level_(LogLevel::Debug)
            , log_file_(nullptr)

        {
        }

        ~log_imp()
        {
        }

        void wait()
        {
            if (bexit_)
            {
                return;
            }

            while(log_queue_.size() > 0)
            {
                thread_sleep(1);
            }
            bexit_ = true;
            log_queue_.exit();

            if(thread_.joinable())
                thread_.join();

            if (nullptr != log_file_)
            {
                fflush(log_file_);
                fclose(log_file_);
                log_file_ = nullptr;
            }
        }

        void write()
        {
            std::vector<std::shared_ptr<log_context>> swaps;
            do
            {
                swaps.clear();
                log_queue_.swap(swaps);
                for(auto& it: swaps)
                {
                    if (it->bconsole_)
                    {
                        auto s = moon::string_view_t(reinterpret_cast<const char*>(it->content_.data()),it->content_.size());
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

                    if (nullptr != log_file_)
                    {
                        fwrite(reinterpret_cast<const void*>(it->content_.data()), it->content_.size(), 1, log_file_);
                        fflush(log_file_);
                    }
                }
            } while (!bexit_ || log_queue_.size()!=0);
        }
    };

    log::log()
        :imp_(new log::log_imp)
    {
    }

    log::~log()
    {
        imp_->wait();
        if (nullptr != imp_)
            delete imp_;
    }

    void log::init(const std::string& logfile)
    {
        bool bfile = !logfile.empty();

        if (bfile)
        {
            if (nullptr != imp_->log_file_)
            {
                std::cout << termcolor::red << "log file already opened." << logfile << std::endl;
                return;
            }

            auto dir = path::directory(logfile);
            if (dir.size() != 0)
            {
                if (!path::exist(dir))
                {
                    path::create_directory(dir);
                }
            }

#if TARGET_PLATFORM == PLATFORM_WINDOWS
            imp_->log_file_ = _fsopen(logfile.data(), "w+", _SH_DENYWR);
#else
            imp_->log_file_ = fopen(logfile.data(), "w+");
#endif //  #if TARGET_PLATFORM == PLATFORM_WINDOWS
            if (nullptr == imp_->log_file_)
            {
                std::cout << termcolor::red << "can not open log file " << logfile << std::endl;
                return;
            }
        }

        imp_->bexit_ = false;
        imp_->thread_ = std::thread(std::bind(&log_imp::write, imp_));
    }

    void log::logfmt(bool console, LogLevel level, const char* fmt, ...)
    {
        if (imp_->level_ < level)
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

    void log::logstring(bool console, LogLevel level, moon::string_view_t s)
    {
        if (imp_->level_ < level)
        {
            return;
        }

        if (s.size()==0)
            return;
        auto ctx = imp_->buffer_cache_.create(console, level);
        moon::buffer* buf = &ctx->content_;
        format_header(buf, level);
        buf->write_back(s.data(), 0, s.size());
        buf->write_back("\n", 0, 1);
        imp_->log_queue_.push_back(ctx);
    }

    void log::set_level(LogLevel level)
    {
        imp_->level_ = level;
    }

    void log::set_level(string_view_t s)
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

    void log::wait()
    {
        imp_->wait();
    }
}

