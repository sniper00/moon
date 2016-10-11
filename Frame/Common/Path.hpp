/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once
#include "MacroDefine.h"
#include "StringUtils.hpp"
#if TARGET_PLATFORM == PLATFORM_WINDOWS
#define PATH_SEPARATOR  ('\\')
#include <io.h>
#include <direct.h>
#else
#define PATH_SEPARATOR  ('/')
#include <sys/stat.h> 
#include <sys/types.h>
#include <unistd.h>    //accesss()
#include <dirent.h>    // DIR
#include <cstring>
#endif
namespace moon
{
	class Path
	{
	public:
		static void AddPathSeparator(std::string& path)
		{
			if (path.size() == 0)
				return;
			if (path.back() != PATH_SEPARATOR)
			{
				path += PATH_SEPARATOR;
			}
		}

		static std::string GetCurrentDir()
		{
			char path[1024] = { 0 };
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

		static bool Exist(const std::string& path)
		{
#if TARGET_PLATFORM == PLATFORM_WINDOWS
			return 	(_access(path.data(), 0) == 0);
#else
			return 	(access(path.data(), 0) == 0);
#endif
		}

		typedef std::function<bool(const std::string&, int)> TraverFileHandler;
		static void TraverseFolder(std::string szPath, int nDepth, const TraverFileHandler& handler)
		{
			if (nDepth < 0)
			{
				return;
			}

			AddPathSeparator(szPath);//make sure end with path separator
#if TARGET_PLATFORM == PLATFORM_WINDOWS

			if (_access(szPath.data(), 0) != 0)
			{
				throw std::runtime_error(string_utils::format("Path[%s] does not exist.", szPath.data()).data());
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
				string_utils::replace(filename, "/", "\\");
				if (fileinfo.attrib&_A_SUBDIR)
				{
					if ((strcmp(fileinfo.name, (".")) != 0) && (strcmp(fileinfo.name, ("..")) != 0))
					{
						handler(filename, 0);
						TraverseFolder(filename, nDepth, handler);
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
	throw std::runtime_error(string_utils::format("Path[%s] does not exist.", szPath.data()).data());
}

DIR *dp;
struct dirent *dirp;
struct stat   filestat;

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
			TraverseFolder(filename, nDepth, handler);
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

		static bool CreateDirectory(std::string path)
		{
#if TARGET_PLATFORM == PLATFORM_WINDOWS
			string_utils::replace(path, "/", "\\");
#endif
			AddPathSeparator(path);
			std::string tmp;
			for (auto c : path)
			{
				if (c == PATH_SEPARATOR)
				{
					if (!Exist(tmp))
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

		static std::string GetExtension(std::string path)
		{
#if TARGET_PLATFORM == PLATFORM_WINDOWS
			string_utils::replace(path, "/", "\\");
#endif
			auto dotpos = path.rfind('.');
			if (dotpos != std::string::npos)
			{
				if (std::string::npos != path.substr(dotpos).find(PATH_SEPARATOR))
				{
					return "";
				}
				return  path.substr(dotpos);
			}
			return "";
		}

		static std::string GetName(std::string path)
		{
#if TARGET_PLATFORM == PLATFORM_WINDOWS
			string_utils::replace(path, "/", "\\");
#endif
			auto tmp = path.rfind(PATH_SEPARATOR);
			if (std::string::npos != tmp)
			{
				return path.substr(tmp+1);
			}
			return "";
		}

		static std::string GetDirectory(std::string path)
		{
#if TARGET_PLATFORM == PLATFORM_WINDOWS
			string_utils::replace(path, "/", "\\");
#endif
			auto tmp = path.rfind(PATH_SEPARATOR);
			if (std::string::npos != tmp)
			{
				return path.substr(0,tmp);
			}
			return "";
		}

		static std::string GetNameWithoutExtension(std::string path)
		{
#if TARGET_PLATFORM == PLATFORM_WINDOWS
			string_utils::replace(path, "/", "\\");
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
	};
}

