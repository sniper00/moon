/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include "macro_define.hpp"
#include "string.hpp"
#include <filesystem>

namespace moon
{
class directory
{
  public:
	static std::string current_directory()
	{
		std::error_code ec;
		auto p = std::filesystem::current_path(ec);
		return p.string();
	}

	static bool exists(const std::string &path)
	{
		std::error_code ec;
		auto b = std::filesystem::exists(path, ec);
		return (!ec)&&b;
	}

	//THandler bool(const std::filesystem::path& path,bool dir)
	template<typename THandler>
	static void traverse_folder(const std::string& dir, int depth, THandler&&handler)
	{
		if (depth < 0)
		{
			return;
		}

		if (!std::filesystem::exists(dir))
		{
			return;
		}

		depth--;

		for (auto&p : std::filesystem::directory_iterator(dir))
		{
			if (!handler(p.path(),p.is_directory()))
			{
				break;
			}
			if (p.is_directory())
			{
				traverse_folder(p.path().lexically_normal().string(), depth, std::forward<THandler>(handler));
			}
		}
	}

	static bool create_directory(const std::string& dir)
	{
		std::error_code ec;
		std::filesystem::create_directories(dir, ec);
		return !ec;
	}

    static bool remove(const std::string& dir)
    {
		std::error_code ec;
		std::filesystem::remove(dir, ec);
		return !ec;
    }

	static bool remove_all (const std::string& dir)
	{
		std::error_code ec;
		std::filesystem::remove_all(dir, ec);
		return !ec;
	}

	static std::string find_file(const std::string& path, const std::string& filename,int depth = 10)
	{
		std::string result;
		traverse_folder(path, depth, [&result,&filename](const std::filesystem::path& p,bool isdir)
		{
			if (!isdir)
			{
				if (p.filename().string() == filename)
				{
					result = p.lexically_normal().string();
					return false;
				}
			}
			return true;
		});
		return result;
	}
};
}
