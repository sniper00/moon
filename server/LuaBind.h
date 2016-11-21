#pragma once
#include <string>
#include "common/noncopyable.hpp"

namespace sol
{
	class state;
}

class LuaBind:public moon::noncopyable
{
public:
	LuaBind(sol::state& lua);
	~LuaBind();

	LuaBind& BindTime();

	LuaBind& BindTimer();

	LuaBind& BindUtil();

	LuaBind& BindPath();

	LuaBind& BindLog();

	LuaBind& BindMessageType();

	LuaBind& BindMessage();

	LuaBind& BindService();

	LuaBind& BindServicePool();

private:
	sol::state& lua;
};

struct lua_State;

std::string Traceback(lua_State* _state);
