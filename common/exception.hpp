
#pragma once
#include <exception>
#include <string>
#include <cstring>
namespace moon
{
    class error : public std::runtime_error {
    private:
        std::string w;
    public:
        error(const std::string& str) : std::runtime_error(""), w(str) {}
        error(std::string&& str) : std::runtime_error(""), w(std::move(str)) {}

        error(const std::string& str, const char* file, int line)
            : std::runtime_error(""), w(str)
        {
            w.append("(");
            w.append(file);
            w.append(":");
            w.append(std::to_string(line));
            w.append(")");
        }

        error(std::string&& str, const char* file, int line)
            : std::runtime_error(""), w(std::move(str))
        {
            w.append("(");
            w.append(file);
            w.append(":");
            w.append(std::to_string(line));
            w.append(")");
        }

        error(const error& e) = default;
        error(error&& e) = default;
        error& operator=(const error& e) = default;
        error& operator=(error&& e) = default;

        virtual const char* what() const noexcept override {
            return w.c_str();
        }
    };
}

#if TARGET_PLATFORM == PLATFORM_WINDOWS
#define __FILENAME__ (strrchr(__FILE__, '\\') ? (strrchr(__FILE__, '\\') + 1):__FILE__)
#else
#define __FILENAME__ (strrchr(__FILE__, '/') ? (strrchr(__FILE__, '/') + 1):__FILE__)
#endif

#define MOON_CHECK(cnd,msg) {if(!(cnd)) throw moon::error{(msg),__FILENAME__,__LINE__};}

#ifdef DEBUG
#define MOON_ASSERT(cnd,msg) assert(cnd && msg);
#else
#define MOON_ASSERT(cnd,msg)
#endif