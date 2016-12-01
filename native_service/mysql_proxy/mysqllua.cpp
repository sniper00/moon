#include "mysqllua.h"
#include "sol.hpp"
#include "MysqlProxy.h"

using namespace moon;

static void BindMysqlProxy(lua_State * L)
{
	sol::state_view lua(L);
	sol::table tb = lua.create_named_table("Mysql");
	tb.set_function("Create", []() { return std::make_shared<MysqlProxy>();});
}

int luaopen_mysql_proxy_c(lua_State * L)
{
	BindMysqlProxy(L);
	return 1;
}
