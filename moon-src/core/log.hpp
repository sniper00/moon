#pragma once
#include "common/concurrent_queue.hpp"
#include "common/time.hpp"
#include "common/termcolor.hpp"
#include "common/directory.hpp"
#include "common/buffer.hpp"
#include "common/string.hpp"
#include "config.hpp"

namespace moon
{
    enum class LogLevel:char
    {
        Error = 1,
        Warn,
        Info,
        Debug,
        Max
    };

    class log
    {
        using queue_type = concurrent_queue<buffer, std::mutex, std::vector>;
    public:
        log()
            :state_(state::init)
            , level_(LogLevel::Debug)
            , thread_(&log::write, this)
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
                std::error_code ec;
                auto parent_path = fs::path(logfile).parent_path();
                if (!parent_path.empty() && !fs::exists(parent_path, ec))
                {
                    fs::create_directories(parent_path, ec);
                    MOON_CHECK(!ec, ec.message().data());
                }
                ofs_.reset(new std::ofstream());
                ofs_->open(logfile, std::ofstream::out | std::ofstream::trunc);
            }
            state_.store(state::ready, std::memory_order_release);
        }

        template<typename... Args>
        void logfmt(bool console, LogLevel level, const char* fmt, Args&&... args)
        {
            if (level_ < level)
            {
                return;
            }

            if (nullptr == fmt)
                return;

            std::string str = moon::format(fmt, std::forward<Args>(args)...);
            logstring(console, level, std::string_view{ str });
        }

        buffer make_line(bool enable_stdout, LogLevel level, uint64_t serviceid = 0, size_t datasize = 0)
        {
            if (level == LogLevel::Error)
                ++error_count_;

            enable_stdout = enable_stdout_ ? enable_stdout : enable_stdout_;

            auto line = buffer{ (datasize>0)?(64 + datasize):256 };
            auto it = line.begin();
            *(it++) = static_cast<char>(enable_stdout);
            *(it++) = static_cast<char>(level);
            size_t offset = format_header(std::addressof(*it), level, serviceid);
            line.commit(2 + offset);
            return line;
        }

        void push_line(buffer line)
        {
            log_queue_.push_back(std::move(line));
            ++size_;
        }

        void logstring(bool console, LogLevel level, std::string_view s, uint64_t serviceid = 0)
        {
            if (level_ < level)
                return;

            buffer line = make_line(console, level, serviceid, s.size());
            line.write_back(s.data(), s.size());
            log_queue_.push_back(std::move(line));
            ++size_;
        }

        void set_level(LogLevel level)
        {
            level_ = level;
        }

		LogLevel get_level() 
		{ 
			return level_; 
		}

        void set_enable_console(bool v)
        {
            enable_stdout_ = v;
        }

        void set_level(std::string_view s)
        {
            if (moon::iequal_string(s, std::string_view{ "DEBUG" }))
            {
                set_level(LogLevel::Debug);
            }
            else  if (moon::iequal_string(s, std::string_view{ "INFO" }))
            {
                set_level(LogLevel::Info);
            }
            else  if (moon::iequal_string(s, std::string_view{ "WARN" }))
            {
                set_level(LogLevel::Warn);
            }
            else  if (moon::iequal_string(s, std::string_view{ "ERROR" }))
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
            if (state_.exchange(state::stopped, std::memory_order_acq_rel) == state::stopped)
                return;

            if (thread_.joinable())
                thread_.join();

            if (ofs_&&ofs_->is_open())
            {
                ofs_->flush();
                ofs_->close();
                ofs_ = nullptr;
            }
        }

        size_t size() const
        {
            return size_.load();
        }

        int64_t error_count() const
        {
            return error_count_;
        }
    private:
        size_t format_header(char* buf, LogLevel level, uint64_t serviceid) const
        {
            size_t offset = 0;
            offset += time::milltimestamp(time::now(), buf, 23);
            memcpy(buf + offset, " | ", 3);
            offset += 3;
            size_t len = 0;
            if (serviceid == 0)
            {
                len = moon::uint64_to_str(moon::thread_id(), buf + offset);
            }
            else
            {
                buf[offset] = ':';
                len = moon::uint64_to_hexstr(serviceid, buf + offset+1,8)+1;
            }
            offset += len;
            if (len < 9)
            {
                memcpy(buf + offset, "        ", 9 - len);
                offset += 9 - len;
            }
            std::string_view strlevel = to_string(level);
            memcpy(buf + offset, strlevel.data(), strlevel.size());
            offset += strlevel.size();
            return offset;
        }

        void do_write(queue_type::container_type& datas)
        {
            for (auto& it : datas)
            {
                auto p = it.data();
                auto bconsole = static_cast<bool>(*(p++));
                auto level = static_cast<LogLevel>(*(p++));
                it.seek(2, buffer::seek_origin::Current);
                if (bconsole)
                {
                    auto s = std::string_view{ it.data(), it.size() };
                    switch (level)
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
                    std::cout << termcolor::white << std::endl;
                }

                if (ofs_)
                {
                    ofs_->write(it.data(), it.size());
                    ofs_->put('\n');
                    if(level <= LogLevel::Error)
                        ofs_->flush();
                }
                --size_;
            }
            if (ofs_)
            {
                ofs_->flush();
            }
        }

        void write()
        {
            while (state_.load(std::memory_order_acquire) == state::init)
                std::this_thread::sleep_for(std::chrono::microseconds(50));

            queue_type::container_type swaps;
            while (state_.load(std::memory_order_acquire) == state::ready)
            {
                if (log_queue_.try_swap(swaps))
                {
                    do_write(swaps);
                    swaps.clear();
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                }
            }

            if(log_queue_.try_swap(swaps))
            {
                do_write(swaps);
                swaps.clear();
            }
        }

        static constexpr std::string_view to_string(LogLevel lv)
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

        bool enable_stdout_ = true;
        std::atomic<state> state_;
        std::atomic_uint32_t size_ = 0;
        std::atomic<LogLevel> level_;
        int64_t error_count_ = 0;
        std::unique_ptr<std::ofstream > ofs_;
        std::thread thread_;
        queue_type log_queue_;
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


