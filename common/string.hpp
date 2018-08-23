/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <algorithm>
#include <locale>
#include <cmath>
#include <sstream>
#include "macro_define.hpp"

#define MAX_FMT_LEN 8*1024

namespace moon
{
	template<class T>
	T string_convert(const string_view_t& s);

    template<>
    inline std::string string_convert< std::string>(const string_view_t& s)
    {
        return std::string(s.data(), s.size());
    }

    template<>
    inline string_view_t string_convert<string_view_t>(const string_view_t& s)
    {
        return s;
    }

	template<>
	inline int32_t string_convert<int32_t>(const string_view_t& s)
	{
        return static_cast<int32_t>(std::stoi(std::string(s.data(),s.size())));
	}

	template<>
	inline uint32_t string_convert<uint32_t>(const string_view_t& s)
	{
		return static_cast<uint32_t>(std::stoul(std::string(s.data(), s.size())));
	}

	template<>
	inline int64_t string_convert<int64_t>(const string_view_t& s)
	{
		return std::stoll(std::string(s.data(), s.size()));
	}

	template<>
	inline uint64_t string_convert<uint64_t>(const string_view_t& s)
	{
		return std::stoull(std::string(s.data(), s.size()));
	}

	template<>
	inline float string_convert<float>(const string_view_t& s)
	{
		return std::stof(std::string(s.data(), s.size()));
	}

	template<>
	inline double string_convert<double>(const string_view_t& s)
	{
		return std::stod(std::string(s.data(), s.size()));
	}

    constexpr uint64_t pow10(int n)
    {
        return (n < 1) ? 1 : 10 * pow10(n - 1);
    }

    inline size_t uint64_to_str(uint64_t value, char *dst)
    {
        static const char digits[201] =
            "0001020304050607080910111213141516171819"
            "2021222324252627282930313233343536373839"
            "4041424344454647484950515253545556575859"
            "6061626364656667686970717273747576777879"
            "8081828384858687888990919293949596979899";
        const  size_t length = static_cast<size_t>(std::log10(value))+1;
        size_t next = length - 1;
        while (value >= 100) {
            const int i = (value % 100) * 2;
            value /= 100;
            dst[next] = digits[i + 1];
            dst[next - 1] = digits[i];
            next -= 2;
        }
        // Handle last 1-2 digits
        if (value < 10) {
            dst[next] = '0' + static_cast<char>(uint32_t(value));
        }
        else {
            const int i = uint32_t(value) * 2;
            dst[next] = digits[i + 1];
            dst[next - 1] = digits[i];
        }
        return length;
    }

	/*
	e. split("aa/bb/cc","/")
	*/
	template<class T>
	std::vector<T> split(const string_view_t& src, const string_view_t& sep)
	{
		std::vector<T> r;
        string_view_t::const_iterator b = src.begin();
        string_view_t::const_iterator e  = src.end();
		for (auto i = src.begin(); i != src.end(); ++i)
		{
			if (sep.find(*i) != string_view_t::npos)
			{
				if (b!= e && b!= i)
				{
					r.push_back(string_convert<T>(string_view_t(std::addressof(*b), size_t(i-b))));
				}
                b = e;
			}
            else
            {
                if (b == e)
                {
                    b = i;
                }
            }
		}
		if (b != e) r.push_back(string_convert<T>(string_view_t(std::addressof(*b), size_t(e - b))));
		return std::move(r);
	}

	//format string
	inline std::string				format(const char* fmt, ...)
	{
		if (!fmt) return std::string("");
		va_list ap;
		char szBuffer[MAX_FMT_LEN];
		va_start(ap, fmt);
		// win32
#if TARGET_PLATFORM == PLATFORM_WINDOWS
		vsnprintf_s(szBuffer, MAX_FMT_LEN, fmt, ap);
#else
		vsnprintf(szBuffer, MAX_FMT_LEN, fmt, ap);
#endif
		va_end(ap);
		return std::string(szBuffer);
	}

	//return left n char
	inline std::string				left(const std::string &str, size_t n)
	{
		return std::string(str, 0, n);
	}

	//return right n char
	inline std::string				right(const std::string &str, size_t n)
	{
		size_t s = (str.size()>=n)?(str.size() - n):0;
		return std::string(str, s);
	}

	//" /t/n/r"
	inline string_view_t trim_right(string_view_t v)
	{
		const auto words_end(v.find_last_not_of(" \t\n\r"));
		if (words_end != string_view_t::npos) {
			v.remove_suffix(v.size() - words_end - 1);
		}
		return v;
	}

	inline string_view_t trim_left(string_view_t v)
	{
		const auto words_begin(v.find_first_not_of(" \t\n\r"));
		v.remove_prefix(std::min(words_begin, v.size()));
		return v;
	}

	inline string_view_t trim_surrounding(string_view_t v)
	{
		const auto words_end(v.find_last_not_of(" \t\n\r"));
		if (words_end != string_view_t::npos) {
			v.remove_suffix(v.size() - words_end - 1);
		}
		const auto words_begin(v.find_first_not_of(" \t\n\r"));
		v.remove_prefix(std::min(words_begin, v.size()));
		return v;
	}

	inline void replace(std::string& src, string_view_t old, string_view_t strnew)
	{
		for (std::string::size_type pos(0); pos != std::string::npos; pos += strnew.size()) {
			if ((pos = src.find(old, pos)) != std::string::npos)
				src.replace(pos, old.size(), strnew);
			else
				break;
		}
	}

	inline void upper(std::string& src)
	{
		std::transform(src.begin(), src.end(), src.begin(), ::toupper);
	}

	inline void lower(std::string& src)
	{
		std::transform(src.begin(), src.end(), src.begin(), ::tolower);
	}

	//! case insensitive
	inline bool iequal_string_locale(const std::string&str1, const std::string& str2, const std::locale& Loc = std::locale())
	{
		if (str1.size() != str2.size())
			return false;

		auto iter1begin = str1.begin();
		auto iter2begin = str2.begin();

		auto iter1end = str1.end();
		auto iter2end = str2.end();

		for (;iter1begin != iter1end && iter2begin != iter2end; ++iter1begin, ++iter2begin)
		{
			if (!(std::toupper(*iter1begin, Loc) == std::toupper(*iter2begin, Loc)))
			{
				return false;
			}
		}
		return true;
	}

    template<typename TString>
	inline bool iequal_string(const TString&str1, const TString& str2)
	{
		if (str1.size() != str2.size())
			return false;

		auto iter1begin = str1.begin();
		auto iter2begin = str2.begin();

		auto iter1end = str1.end();
		auto iter2end = str2.end();

		for (;iter1begin != iter1end && iter2begin != iter2end; ++iter1begin, ++iter2begin)
		{
			if (!(toupper(*iter1begin) == toupper(*iter2begin)))
			{
				return false;
			}
		}
		return true;
	}

	inline std::string hex_string(string_view_t s,string_view_t tok = "")  
	{  
		std::stringstream ss;
		ss<<std::setiosflags(std::ios::uppercase)<<std::hex;
        for (auto c : s)
        {
            ss << (int)(uint8_t)(c) << tok;
        }
		return ss.str();  
	}

    template<typename TString>
	struct ihash_string_functor
	{
		size_t operator()(const TString &str) const noexcept
		{
			size_t h = 0;
			std::hash<int> hash;
			for (auto c : str)
				h ^= hash(tolower(c)) + 0x9e3779b9 + (h << 6) + (h >> 2);
			return h;
		}
	};

    using ihash_string_functor_t = ihash_string_functor<std::string>;
    using ihash_string_view_functor_t = ihash_string_functor<string_view_t>;

    template<typename TString>
	struct iequal_string_functor
	{
		bool operator()(const TString &str1, const TString &str2) const noexcept
		{
			return iequal_string(str1, str2);
		}
	};

    using iequal_string_functor_t = iequal_string_functor<std::string>;
    using iequal_string_view_functor_t = iequal_string_functor<string_view_t>;

	inline constexpr size_t chash_string(const char* str, size_t seed = 0)
	{
		return 0 == *str ? seed : chash_string(str + 1, seed ^ (static_cast<size_t>(*str) + 0x9e3779b9 + (seed << 6) + (seed >> 2)));
	}
};
