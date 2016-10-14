// EchoExample.cpp : 定义控制台应用程序的入口点。
//

#include "sol.hpp"
#include "Detail/Log/Log.h"
#include "MoonNetLuaBind.h"
#include "Common/Path.hpp"

int main()
{
	sol::state lua;
	try
	{
		lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::io, sol::lib::table, sol::lib::string, sol::lib::debug);
		MoonNetLuaBind luaBind(lua);
		luaBind.BindModuleManager()
			.BindEMessageType()
			.BindPath();

#if TARGET_PLATFORM == PLATFORM_WINDOWS
		lua.script("package.cpath = './Lib/?.dll;'");
#else
		lua.script("package.cpath = './Lib/?.so;'");
#endif

		lua.script_file("main.lua");
	}
	catch (sol::error& e)
	{
		CONSOLE_ERROR("%s", e.what());
		CONSOLE_DEBUG("Traceback: %s", Traceback(lua.lua_state()).data());
	}

	return 0;
}

