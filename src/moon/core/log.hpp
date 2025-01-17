#pragma once
#include "common/buffer.hpp"
#include "common/concurrent_queue.hpp"
#include "common/directory.hpp"
#include "common/string.hpp"
#include "common/termcolor.hpp"
#include "common/time.hpp"
#include "config.hpp"

namespace moon {
enum class LogLevel : char { Error = 1, Warn, Info, Debug, Max };

class log final {
    using queue_type = concurrent_queue<buffer, std::mutex, std::vector>;

    log(): thread_(&log::write, this) {}

public:
    static log& instance() {
        static log inst;
        return inst;
    }

    ~log() {
        wait();
    }

    log(const log&) = delete;

    log& operator=(const log&) = delete;

    void init(const std::string& logfile) {
        if (!logfile.empty()) {
            std::error_code ec;
            if (auto parent_path = fs::path(logfile).parent_path();
                !parent_path.empty() && !fs::exists(parent_path, ec))
            {
                fs::create_directories(parent_path, ec);
                MOON_CHECK(!ec, ec.message().data());
            }

            int err = 0;
#if TARGET_PLATFORM == PLATFORM_WINDOWS
            FILE* fp = _fsopen(logfile.data(), "w", _SH_DENYWR);
            if (nullptr == fp)
                err = errno;
#else
            FILE* fp = std::fopen(logfile.data(), "w");
            if (nullptr == fp)
                err = errno;
#endif
            MOON_CHECK(
                !err,
                moon::format("open log file '%s' failed. errno %d.", logfile.data(), err)
            )
            fp_.reset(fp);
        }
        state_.store(state::ready, std::memory_order_release);
    }

    bool is_ready() const {
        return state_.load(std::memory_order_acquire) == state::ready;
    }

    template<typename... Args>
    void logfmt(bool console, LogLevel level, const char* fmt, Args&&... args) {
        if (level_ < level) {
            return;
        }

        if (nullptr == fmt)
            return;

        std::string str = moon::format(fmt, std::forward<Args>(args)...);
        logstring(console, level, std::string_view { str });
    }

    buffer
    make_line(bool enable_stdout, LogLevel level, uint64_t serviceid = 0, size_t datasize = 0) {
        if (level == LogLevel::Error)
            ++error_count_;

        enable_stdout = enable_stdout_ ? enable_stdout : enable_stdout_;

        auto line = buffer { (datasize > 0) ? (64 + datasize) : 128 };
        auto it = line.begin();
        *(it++) = static_cast<char>(enable_stdout);
        *(it++) = static_cast<char>(level);
        size_t offset = format_header(std::addressof(*it), level, serviceid);
        line.commit(2 + offset);
        return line;
    }

    void push_line(buffer line) {
        log_queue_.push_back(std::move(line));
        ++size_;
    }

    void logstring(bool console, LogLevel level, std::string_view s, uint64_t serviceid = 0) {
        try {
            if (level_ < level)
                return;

            buffer line = make_line(console, level, serviceid, s.size());
            line.write_back(s.data(), s.size());
            log_queue_.push_back(std::move(line));
            ++size_;
        } catch (const std::exception& e) {
            std::cerr << "logstring exception:" << e.what() << std::endl;
        }
    }

    void set_level(LogLevel level) {
        level_ = level;
    }

    LogLevel get_level() const {
        return level_;
    }

    void set_enable_console(bool v) {
        enable_stdout_ = v;
    }

    void set_level(std::string_view s) {
        if (moon::iequal_string(s, std::string_view { "DEBUG" })) {
            set_level(LogLevel::Debug);
        } else if (moon::iequal_string(s, std::string_view { "INFO" })) {
            set_level(LogLevel::Info);
        } else if (moon::iequal_string(s, std::string_view { "WARN" })) {
            set_level(LogLevel::Warn);
        } else if (moon::iequal_string(s, std::string_view { "ERROR" })) {
            set_level(LogLevel::Error);
        } else {
            set_level(LogLevel::Debug);
        }
    }

    void wait() {
        if (state_.exchange(state::stopped, std::memory_order_acq_rel) == state::stopped)
            return;

        if (thread_.joinable())
            thread_.join();

        fp_.reset(nullptr);
    }

    size_t size() const {
        return size_.load();
    }

    size_t error_count() const {
        return error_count_;
    }

private:
    size_t format_header(char* buf, LogLevel level, uint64_t serviceid) const {
        size_t offset = 0;
        // Format the timestamp
        offset += time::milltimestamp(time::now(), buf, 23);

        std::string_view strlevel = to_string(level);
        memcpy(buf + offset, strlevel.data(), strlevel.size());
        offset += strlevel.size();

        // Format the service ID or thread ID
        size_t len = 0;
        if (serviceid == 0) {
            len = moon::uint64_to_str(moon::thread_id(), buf + offset);
        } else {
            buf[offset] = ':';
            len = moon::uint64_to_hexstr(serviceid, buf + offset + 1, 8) + 1;
        }
        offset += len;

        // Pad with spaces if necessary
        if (len < 9) {
            memset(buf + offset, ' ', 9 - len);
            offset += 9 - len;
        }
        buf[offset++] = '|';
        buf[offset++] = ' ';

        return offset;
    }

    void do_write(const queue_type::container_type& lines) {
        for (auto& it: lines) {
            auto p = it.data();
            auto bconsole = static_cast<bool>(*(p++));
            auto level = static_cast<LogLevel>(*(p++));
            auto str = std::string_view { p, it.size() - 2 };
            std::ostream* stream = nullptr;
            if (bconsole) {
                switch (level) {
                    case LogLevel::Error:
                        stream = &std::cerr;
                        *stream << termcolor::red;
                        break;
                    case LogLevel::Warn:
                        stream = &std::cout;
                        *stream << termcolor::yellow;
                        break;
                    case LogLevel::Info:
                        stream = &std::cout;
                        *stream << termcolor::white;
                        break;
                    case LogLevel::Debug:
                        stream = &std::cout;
                        *stream << termcolor::green;
                        break;
                    default:
                        break;
                }
                if (stream != nullptr) {
                    *stream << str << termcolor::white;
                    if (str.back() != '\n')
                        *stream << '\n';
                }
            }

            if (fp_) {
                std::fwrite(str.data(), str.size(), 1, fp_.get());
                if (str.back() != '\n')
                    std::fputc('\n', fp_.get());
                if (level <= LogLevel::Error) {
                    std::cout << std::endl;
                    std::fflush(fp_.get());
                }
            }
            --size_;
        }
        if (fp_) {
            std::fflush(fp_.get());
        }
        std::cout.flush();
    }

    void write() {
        while (state_.load(std::memory_order_acquire) == state::init)
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        time_t sleep_time = 1;
        while (state_.load(std::memory_order_acquire) == state::ready) {
            if (auto& read_queue = log_queue_.swap_on_read(); !read_queue.empty()) {
                sleep_time = 1;
                do_write(read_queue);
                read_queue.clear();
            } else {
                std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_time));
                sleep_time = (sleep_time << 1);
                sleep_time = std::min(sleep_time, (time_t)1 << 24);
            }
        }

        if (auto& read_queue = log_queue_.swap_on_read(); !read_queue.empty()) {
            do_write(read_queue);
            read_queue.clear();
        }
    }

    static constexpr std::string_view to_string(LogLevel lv) {
        switch (lv) {
            case LogLevel::Error:
                return " EROR|";
            case LogLevel::Warn:
                return " WARN|";
            case LogLevel::Info:
                return " INFO|";
            case LogLevel::Debug:
                return " DBUG|";
            default:
                return " NULL|";
        }
    }

    struct file_deleter {
        void operator()(FILE* p) const {
            if (nullptr == p)
                return;
            std::fflush(p);
            std::fclose(p);
        }
    };

    bool enable_stdout_ = true;
    std::atomic<state> state_ = state::init;
    std::atomic_uint32_t size_ = 0;
    std::atomic<LogLevel> level_ = LogLevel::Debug;
    size_t error_count_ = 0;
    std::unique_ptr<std::FILE, file_deleter> fp_;
    std::thread thread_;
    queue_type log_queue_;
};
} // namespace moon

#define CONSOLE_INFO(fmt, ...) \
    moon::log::instance().logfmt(true, moon::LogLevel::Info, fmt, ##__VA_ARGS__)
#define CONSOLE_WARN(fmt, ...) \
    moon::log::instance() \
        .logfmt(true, moon::LogLevel::Warn, fmt " (%s:%d)", ##__VA_ARGS__, __FILENAME__, __LINE__)
#define CONSOLE_ERROR(fmt, ...) \
    moon::log::instance().logfmt( \
        true, \
        moon::LogLevel::Error, \
        fmt " (%s:%d)", \
        ##__VA_ARGS__, \
        __FILENAME__, \
        __LINE__ \
    )
#define CONSOLE_DEBUG(fmt, ...) \
    moon::log::instance().logfmt( \
        true, \
        moon::LogLevel::Debug, \
        fmt " (%s:%d)", \
        ##__VA_ARGS__, \
        __FILENAME__, \
        __LINE__ \
    )
