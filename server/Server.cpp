// EchoExample.cpp : 定义控制台应用程序的入口点。
//

#include "sol.hpp"
#include "LuaBind.h"
#include "service_pool.h"
#include "log.h"

int main()
{
	moon::service_pool  pool;
	sol::state lua;
	try
	{
		lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::io, sol::lib::table, sol::lib::string, sol::lib::debug,sol::lib::coroutine);
		LuaBind luaBind(lua);
		luaBind.BindServicePool()
			.BindMessageType()
			.BindPath()
			.BindTime();

#if TARGET_PLATFORM == PLATFORM_WINDOWS
		lua.script("package.cpath = './Lib/?.dll;'");
#else
		lua.script("package.cpath = './Lib/?.so;'");
#endif

		lua.set("pool", std::ref(pool));
		lua.script_file("main.lua");
	}
	catch (sol::error& e)
	{
		CONSOLE_ERROR("%s", e.what());
		CONSOLE_DEBUG("Traceback: %s", Traceback(lua.lua_state()).data());
	}
	return 0;
}

