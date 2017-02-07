#include "mysqllua.h"
#include "sol.hpp"
#include "MysqlProxy.h"
#include "service_pool.h"

using namespace moon;

static void BindMysqlProxy(lua_State * L)
{
	sol::state_view lua(L);
	sol::table tb = lua.create_named_table("mysql_proxy");

	auto f = [](const std::string& type) {
		service_pool::instance()->register_service(type, []()->service_ptr_t {
			return  std::make_shared<MysqlProxy>();
		});
	};

	tb.set_function("register", f);
}

int luaopen_mysql_proxy(lua_State * L)
{
	BindMysqlProxy(L);
	return 1;
}
