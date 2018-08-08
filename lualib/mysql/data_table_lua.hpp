#pragma once
#include <vector>
#include <variant>
#include <cassert>
#include <string>
#include <tuple>
#include <string_view>
#include"data_table.hpp"
#include "lua.hpp"
namespace db
{
	class data_table_lua
	{
	public:
		data_table_lua(lua_State *L_)
			:L(L_)
		{
			lua_createtable(L, 64, 0);
			lua_table_ = lua_gettop(L);
		}

		template<typename Name, typename Type>
		void add_column(Name&& colname, Type&& data_type)
		{
			cols_.emplace_back(std::forward<Name>(colname), std::forward<Type>(data_type));
		}

		template<typename Row>
		void add_row(Row&& rowdata)
		{
			luaL_checkstack(L, LUA_MINSTACK, NULL);
			lua_createtable(L,static_cast<int>(cols_.size()), 0);

			int i = 0;
			for (auto& col : cols_)
			{
				switch (col.second)
				{
				case MYSQL_TYPE_TINY:
				case MYSQL_TYPE_SHORT:
				case MYSQL_TYPE_LONG:
				case MYSQL_TYPE_LONGLONG:
					lua_pushinteger(L, std::stoll(rowdata[i]));
					break;
				case MYSQL_TYPE_FLOAT:
				case MYSQL_TYPE_DOUBLE:
					lua_pushnumber(L, std::stod(rowdata[i]));
					break;
				case MYSQL_TYPE_VAR_STRING:
				{
					std::string_view s(rowdata[i]);
					lua_pushlstring(L, s.data(), s.size());
					break;
				}
				default:
					break;
				}
				i++;
				lua_rawseti(L, -2, i);
			}
			lua_rawseti(L, lua_table_, ++nrow_);
		}

		const size_t column_size() const
		{
			return cols_.size();
		}

	private:
		int lua_table_ = 0;
		int nrow_ = 0;
		lua_State * L;
		std::vector<std::pair<std::string, int>> cols_;
	};
}