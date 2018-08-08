
#define LUA_LIB

#include <stdlib.h>
#include <string.h>
#include <array>
#include <list>
#include"lua.hpp"
#include "mysql.hpp"
#include "data_table_lua.hpp"

#define METANAME "mysql"

#define MAX_DEPTH 32

using value_holder_t =std::array<char,16>;

static void bind_param_one(lua_State *L,std::vector<MYSQL_BIND>& params, std::list<value_holder_t>& holder, int index)
{
	MYSQL_BIND param = {};

	int type = lua_type(L, index);
	switch (type) {
	case LUA_TNIL:
		param.buffer_type = (enum_field_types)db::type_id_map<nullptr_t>::value;
		param.buffer = (NULL);
		break;
	case LUA_TNUMBER:
	{
		if (lua_isinteger(L, index))
		{
			lua_Integer value = lua_tointeger(L, index);
			auto& buf = holder.emplace_back();
			memcpy(buf.data(), &value, sizeof(value));
			param.buffer_type = (enum_field_types)db::type_id_map<int64_t>::value;
			param.buffer = const_cast<void*>(static_cast<const void*>(buf.data()));
		}
		else
		{
			lua_Number value = lua_tonumber(L, index);
			auto& buf = holder.emplace_back();
			memcpy(buf.data(), &value, sizeof(value));
			param.buffer_type = (enum_field_types)db::type_id_map<double>::value;
			param.buffer = const_cast<void*>(static_cast<const void*>(buf.data()));
		}
		break;
	}
	case LUA_TBOOLEAN:
	{
		char value = static_cast<char>(lua_toboolean(L, index));
		auto& buf = holder.emplace_back();
		memcpy(buf.data(), &value, sizeof(value));
		param.buffer_type = (enum_field_types)db::type_id_map<char>::value;
		param.buffer = const_cast<void*>(static_cast<const void*>(buf.data()));
		break;
	}
	case LUA_TSTRING: {
		size_t sz = 0;
		const char *str = lua_tolstring(L, index, &sz);
		param.buffer_type = MYSQL_TYPE_STRING;
		param.buffer = (void*)(str);
		param.buffer_length = (unsigned long)sz;
		break;
	}
	default:
		luaL_error(L, "Unsupport type %s to concat", lua_typename(L, type));
	}
	params.push_back(param);
}

static int
traceback(lua_State *L) {
	const char *msg = lua_tostring(L, 1);
	if (msg)
		luaL_traceback(L, L, msg, 1);
	else {
		lua_pushliteral(L, "(no error message)");
	}
	return 1;
}

struct lua_mysql_box
{
	db::mysql* mysql;
};

static int lrelease(lua_State *L)
{
	lua_mysql_box* my = (lua_mysql_box*)lua_touserdata(L, 1);
	if (my&&my->mysql)
	{
		delete my->mysql;
		my->mysql = nullptr;
	}
	return 0;
}

static int lmysql_connect(lua_State *L)
{
	lua_mysql_box* my = (lua_mysql_box*)lua_touserdata(L, 1);
	if (my == nullptr || my->mysql == nullptr)
		return luaL_error(L, "Invalid mysql pointer");
	const char* host = luaL_checkstring(L, 2);
	int port = (int)luaL_checkinteger(L, 3);
	const char* user = luaL_checkstring(L, 4);
	const char* password = luaL_checkstring(L, 5);
	const char* database = luaL_checkstring(L, 6);
	int timeout = (int)luaL_checkinteger(L, 7);

	try
	{
		my->mysql->connect(host, port, user, password, database, timeout);
	}
	catch(std::exception& e)
	{
		lua_pushboolean(L, 0);
		lua_pushstring(L, e.what());
		return 2;
	}
	lua_pushboolean(L, 1);
	return 1;
}

static int lmysql_connected(lua_State *L)
{
	lua_mysql_box* my = (lua_mysql_box*)lua_touserdata(L, 1);
	if (my == nullptr || my->mysql == nullptr)
		return luaL_error(L, "Invalid mysql pointer");

	bool connected = my->mysql->connected();
	lua_pushboolean(L, connected?1:0);
	return 1;
}

static int lmysql_errorcode(lua_State *L)
{
	lua_mysql_box* my = (lua_mysql_box*)lua_touserdata(L, 1);
	if (my == nullptr || my->mysql == nullptr)
		return luaL_error(L, "Invalid mysql pointer");
	int ec = my->mysql->error_code();
	lua_pushinteger(L, ec);
	return 1;
}

static int lmysql_reconnect(lua_State *L)
{
	lua_mysql_box* my = (lua_mysql_box*)lua_touserdata(L, 1);
	if (my == nullptr || my->mysql == nullptr)
		return luaL_error(L, "Invalid mysql pointer");
	try
	{
		my->mysql->reconnect();
	}
	catch (std::exception& e)
	{
		lua_pushboolean(L, 0);
		lua_pushstring(L, e.what());
		return 2;
	}
	lua_pushboolean(L, 1);
	return 1;
}

static int lmysql_query(lua_State *L)
{
	struct lua_mysql_box* my = (lua_mysql_box*)lua_touserdata(L, 1);
	if (my == nullptr || my->mysql == nullptr)
		return luaL_error(L, "Invalid mysql pointer");

	const char* sql = luaL_checkstring(L, 2);

	try
	{
		my->mysql->query<db::data_table_lua>(sql,L);
	}
	catch (std::exception& e)
	{
		lua_pushboolean(L, 0);
		lua_pushstring(L, e.what());
		return 2;
	}
	return 1;
}

static int lmysql_execute(lua_State *L)
{
	struct lua_mysql_box* my = (lua_mysql_box*)lua_touserdata(L, 1);
	if (my == nullptr || my->mysql == nullptr)
		return luaL_error(L, "Invalid mysql pointer");

	const char* sql = luaL_checkstring(L, 2);

	try
	{
		my->mysql->execute(sql);
	}
	catch (std::exception& e)
	{
		lua_pushboolean(L, 0);
		lua_pushstring(L, e.what());
		return 2;
	}
	lua_pushboolean(L, 1);
	return 1;
}

static int  lmysql_prepare(lua_State *L)
{
	struct lua_mysql_box* my = (lua_mysql_box*)lua_touserdata(L, 1);
	if (my == nullptr || my->mysql == nullptr)
		return luaL_error(L, "Invalid mysql pointer");

	const char* sql = luaL_checkstring(L, 2);

	try
	{
		auto id = my->mysql->prepare(sql);
		auto strid = std::to_string(id);
		lua_pushlstring(L, strid.data(),strid.size());
	}
	catch (std::exception& e)
	{
		lua_pushboolean(L, 0);
		lua_pushstring(L, e.what());
		return 2;
	}
	return 1;
}

static int lmysql_execute_stmt(lua_State *L)
{
	struct lua_mysql_box* my = (lua_mysql_box*)lua_touserdata(L, 1);
	if (my == nullptr || my->mysql == nullptr)
		return luaL_error(L, "Invalid mysql pointer");

	size_t len = 0;
	const char* stmtid = luaL_checklstring(L, 2, &len);
	std::vector<MYSQL_BIND> params;
	std::list<value_holder_t> holder;

	int n = lua_gettop(L);
	for (int i = 3; i <= n; i++) {
		bind_param_one(L, params, holder, i);
	}

	try
	{
		my->mysql->execute_stmt_param(std::stoull(stmtid),params);
	}
	catch (std::exception& e)
	{
		lua_pushboolean(L, 0);
		lua_pushstring(L, e.what());
		return 2;
	}
	lua_pushboolean(L, 1);
	return 1;
}

static int lmysql_create(lua_State *L)
{
	db::mysql*mysql = new db::mysql;
	struct lua_mysql_box* my = (lua_mysql_box*)lua_newuserdata(L, sizeof(*my));
	my->mysql = mysql;
	if (luaL_newmetatable(L, METANAME))//mt
	{
		luaL_Reg l[] = {
			{ "connect",lmysql_connect },
			{ "connected",lmysql_connected },
			{ "errorcode",lmysql_errorcode },
			{ "reconnect",lmysql_reconnect },
			{ "query",lmysql_query },
			{ "execute",lmysql_execute },
			{ "prepare",lmysql_prepare },
			{ "execute_stmt",lmysql_execute_stmt },
			{ NULL,NULL }
		};
		luaL_newlib(L, l); {}
		lua_setfield(L, -2, "__index");//mt[__index] = {}
		lua_pushcfunction(L, lrelease);
		lua_setfield(L, -2, "__gc");//mt[__gc] = lrelease
	}
	lua_setmetatable(L, -2);// set userdata metatable
	lua_pushlightuserdata(L, my);
	return 2;
}

#if __cplusplus
extern "C" {
#endif
int LUAMOD_API luaopen_mysql(lua_State *L)
{
	luaL_Reg l[] = {
		{"create",lmysql_create },
		{"release",lrelease },
		{NULL,NULL}
	};
	luaL_checkversion(L);
	luaL_newlib(L, l);
	return 1;
}
#if __cplusplus
}
#endif