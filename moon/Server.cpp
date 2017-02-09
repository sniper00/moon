// EchoExample.cpp : 定义控制台应用程序的入口点。
//

#include "sol.hpp"
#include "LuaBind.h"
#include "service_pool.h"
#include "log.h"
#include "common/path.hpp"
#include "common/string.hpp"

int main(int argc,char*argv[])
{
	{
		moon::service_pool  pool;
		{
			sol::state lua;
			try
			{
				lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::io, sol::lib::table, sol::lib::string, sol::lib::debug, sol::lib::coroutine);
				LuaBind luaBind(lua);
				luaBind.BindServicePool()
					.BindMessageType()
					.BindPath()
					.BindTime()
					.BindService();

#if TARGET_PLATFORM == PLATFORM_WINDOWS
				lua.script("package.cpath = './Lib/?.dll;'");
#else
				lua.script("package.cpath = './Lib/?.so;'");
#endif

				lua.set("pool", &pool);

				if (argc == 2)
				{
					const char* startfile = argv[1];
					if (moon::path::extension(startfile) != ".lua")
					{
						throw std::logic_error("startup file must be a lua script.");
					}
					lua.script_file(startfile);
				}
				else
				{
					lua.script_file("main.lua");
				}		
			}
			catch (sol::error& e)
			{
				pool.stop();
				CONSOLE_ERROR("%s", e.what());
				CONSOLE_DEBUG("Traceback: %s", Traceback(lua.lua_state()).data());
			}
		}
	}

	moon::log::wait();
	return 0;
}

