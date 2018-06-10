/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2017 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include "macro_define.hpp"
#include "string.hpp"
#ifdef _WIN32
#define PATH_SEPARATOR ('\\')
#include <io.h>
#include <direct.h>
#else
#define PATH_SEPARATOR ('/')
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h> //accesss()
#include <dirent.h> // DIR
#include <cstring>
#endif
namespace moon
{
class path
{
  public:
	static void add_path_separator(std::string &path)
	{
		if (path.size() == 0)
			return;
		if (path.back() != PATH_SEPARATOR)
		{
			path += PATH_SEPARATOR;
		}
	}

	static std::string current_directory()
	{
		char path[1024] = {0};
#if TARGET_PLATFORM == PLATFORM_WINDOWS
		if (!GetCurrentDirectoryA(1024, path))
		{
			throw std::runtime_error("GetModuleFileName failed");
		}
#else
		if (!getcwd(path, 1024))
		{
			throw std::runtime_error("getcwd failed");
		}
#endif
		return std::string(path);
	}

	static bool exist(const std::string &path)
	{
#if TARGET_PLATFORM == PLATFORM_WINDOWS
		return (_access(path.data(), 0) == 0);
#else
		return (access(path.data(), 0) == 0);
#endif
	}

	//THandler bool(const std::string&filepath,int type) type:0 folder,1 file
	template<typename THandler>
	static void traverse_folder(std::string szPath, int nDepth, THandler&&handler)
	{
		if (nDepth < 0)
		{
			return;
		}

		add_path_separator(szPath); //make sure end with path separator
#if TARGET_PLATFORM == PLATFORM_WINDOWS

		if (_access(szPath.data(), 0) != 0)
		{
			throw std::runtime_error(moon::format("Path[%s] does not exist.", szPath.data()).data());
		}

		nDepth--;

		_finddata_t fileinfo;
		auto lhandle = _findfirst((szPath + "*").data(), &fileinfo);
		if (lhandle == -1)
		{
			return;
		}

		do
		{
			auto filename = szPath + fileinfo.name;
			moon::replace(filename, "/", "\\");
			if (fileinfo.attrib & _A_SUBDIR)
			{
				if ((strcmp(fileinfo.name, (".")) != 0) && (strcmp(fileinfo.name, ("..")) != 0))
				{
					handler(filename, 0);
					traverse_folder(filename, nDepth, handler);
				}
			}
			else
			{
				if (!handler(filename, 1))
				{
					break;
				}
			}
		} while (_findnext(lhandle, &fileinfo) == 0);
		_findclose(lhandle);
#else
		if (access(szPath.data(), 0) != 0)
		{
			throw std::runtime_error(moon::format("Path[%s] does not exist.", szPath.data()).data());
		}

		DIR *dp;
		struct dirent *dirp;
		struct stat filestat;

		if ((dp = opendir(szPath.data())) == NULL)
		{
			return;
		}

		while ((dirp = readdir(dp)) != NULL)
		{
			auto filename = szPath + dirp->d_name;
			lstat(filename.data(), &filestat);
			if (S_ISDIR(filestat.st_mode))
			{
				if (strcmp(dirp->d_name, ".") != 0 && strcmp(dirp->d_name, "..") != 0)
				{
					handler(filename, 0);
					traverse_folder(filename, nDepth, handler);
				}
			}
			else
			{
				if (!handler(filename, 1))
				{
					break;
				}
			}
		}
		closedir(dp);
#endif
		return;
	}

	static bool create_directory(std::string path)
	{
#if TARGET_PLATFORM == PLATFORM_WINDOWS
		moon::replace(path, "/", "\\");
#endif
		add_path_separator(path);
		std::string tmp;
		for (auto c : path)
		{
			if (c == PATH_SEPARATOR)
			{
				if (!exist(tmp))
				{
#if TARGET_PLATFORM == PLATFORM_WINDOWS
					if (_mkdir(tmp.data()) == -1)
#else
					if (mkdir(tmp.data(), 0755) == -1)
#endif
						return false;
				}
			}
			tmp += c;
		}
		return true;
	}

	static std::string extension(std::string path)
	{
#if TARGET_PLATFORM == PLATFORM_WINDOWS
		moon::replace(path, "/", "\\");
#endif
		auto dotpos = path.rfind('.');
		if (dotpos != std::string::npos)
		{
			if (std::string::npos != path.substr(dotpos).find(PATH_SEPARATOR))
			{
				return "";
			}
			return path.substr(dotpos);
		}
		return "";
	}

	static std::string filename(std::string path)
	{
#if TARGET_PLATFORM == PLATFORM_WINDOWS
		moon::replace(path, "/", "\\");
#endif
		auto tmp = path.rfind(PATH_SEPARATOR);
		if (std::string::npos != tmp)
		{
			return path.substr(tmp + 1);
		}
		return "";
	}

	static std::string directory(std::string path)
	{
#if TARGET_PLATFORM == PLATFORM_WINDOWS
		moon::replace(path, "/", "\\");
#endif
		auto tmp = path.rfind(PATH_SEPARATOR);
		if (std::string::npos != tmp)
		{
			return path.substr(0, tmp);
		}
		return "";
	}

	static std::string name_without_extension(std::string path)
	{
#if TARGET_PLATFORM == PLATFORM_WINDOWS
		moon::replace(path, "/", "\\");
#endif
		auto tmp = path.rfind(PATH_SEPARATOR);
		if (std::string::npos != tmp)
		{
			auto dotpos = path.substr(tmp).rfind('.');
			if (std::string::npos != dotpos)
			{
				return path.substr(tmp + 1, dotpos - 1);
			}
			return path.substr(tmp + 1);
		}
		return "";
	}

    static bool remove(const std::string& path)
    {
        return 0== ::remove(path.data());
    }
};
}
