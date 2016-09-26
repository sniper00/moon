/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include <stdarg.h>
#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>

#define MAX_FMT_LEN 32*1024

namespace string_utils
{
	template<class T>
	T string_convert(const std::string& s);

	template<>
	inline std::string string_convert<std::string>(const std::string& s)
	{
		return s;
	}

	template<>
	inline int32_t string_convert<int32_t>(const std::string& s)
	{
		return std::stoi(s);
	}

	template<>
	inline uint32_t string_convert<uint32_t>(const std::string& s)
	{
		return std::stoul(s)&(0xFFFFFFFF);
	}

	template<>
	inline int64_t string_convert<int64_t>(const std::string& s)
	{

		return std::stoll(s);
	}

	template<>
	inline uint64_t string_convert<uint64_t>(const std::string& s)
	{
		return std::stoull(s);
	}

	template<>
	inline float string_convert<float>(const std::string& s)
	{
		return std::stof(s);
	}

	template<>
	inline double string_convert<double>(const std::string& s)
	{
		return std::stod(s);
	}

	inline std::string to_string(const std::string& s)
	{
		return s;
	}

	template<typename T>
	std::string to_string(T t,typename std::enable_if<std::is_integral<T>::value>::type* = nullptr)
	{
		if (std::is_signed<T>::value)
		{
			return std::to_string(int64_t(t));
		}
		else
		{
			return std::to_string(uint64_t(t));
		}
	}

	template<typename T>
	std::string to_string(T t, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr)
	{
		return std::to_string(double(t));
	}

	/*
	e. split("aa/bb/cc","/")
	*/
	template<class T>
	std::vector<T> split(const std::string &src, const std::string &sep)
	{
		std::vector<T> r;
		std::string s;
		for (std::string::const_iterator i = src.begin(); i != src.end(); i++)
		{
			if (sep.find(*i) != std::string::npos)
			{
				if (s.length()) r.push_back(string_convert<T>(s));
				s = "";
			}
			else
			{
				s += *i;
			}
		}
		if (s.length()) r.push_back(string_convert<T>(s));
		return std::move(r);
	}

	//format string
	inline std::string				format(const char* fmt, ...)
	{
		if (!fmt) return std::string("");
		va_list ap;
		char szBuffer[MAX_FMT_LEN];
		va_start(ap, fmt);
		int ret = 0;
		// win32
#if defined(_WIN32)
		ret = vsnprintf_s(szBuffer, MAX_FMT_LEN, fmt, ap);
#endif
		// linux
#if defined(__linux__) || defined(__linux) 
		ret = vsnprintf(szBuffer, MAX_FMT_LEN, fmt, ap);
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
	inline void						trimright(std::string &str)
	{
		str.erase(str.find_last_not_of(" /t/n/r" + 1));
	}

	inline void						trimleft(std::string &str)
	{
		str.erase(str.find_first_not_of(" /t/n/r" + 1));
	}

	inline void						replace(std::string& src, const std::string& old, const std::string& strnew)
	{
		for (std::string::size_type pos(0); pos != std::string::npos; pos += strnew.size()) {
			if ((pos = src.find(old, pos)) != std::string::npos)
				src.replace(pos, old.size(), strnew);
			else
				break;
		}
	}

	inline void						upper(std::string& src)
	{
		std::transform(src.begin(), src.end(), src.begin(), ::toupper);
	}

	inline void						lower(std::string& src)
	{
		std::transform(src.begin(), src.end(), src.begin(), ::tolower);
	}

	inline std::unordered_map<std::string, std::string> parse_key_value_string(const std::string& v)
	{
		std::unordered_map<std::string, std::string> ret;
		auto vec = split<std::string>(v, ";");
		for (auto& it : vec)
		{
			auto pairs = string_utils::split<std::string>(it, ":");
			if (pairs.size() == 2)
			{
				ret.emplace(pairs[0], pairs[1]);
			}
		}
		return ret;
	}
};
