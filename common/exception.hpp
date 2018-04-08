/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <exception>
#include <string>
namespace moon
{
    class error : public std::runtime_error {
    private:
        std::string w;
    public:
        error(const std::string& str) : std::runtime_error(""), w(str) {}
        error(std::string&& str) : std::runtime_error(""), w(std::move(str)) {}

        error(const error& e) = default;
        error(error&& e) = default;
        error& operator=(const error& e) = default;
        error& operator=(error&& e) = default;

        virtual const char* what() const noexcept override {
            return w.c_str();
        }
    };
}

#define MOON_CHECK(cnd,msg) {if(!(cnd)) throw moon::error(msg);}

#ifdef DEBUG
#define MOON_DCHECK(cnd,msg) MOON_CHECK(cnd,msg)
#else
#define MOON_DCHECK(cnd,msg) MOON_CHECK(cnd,msg)
#endif