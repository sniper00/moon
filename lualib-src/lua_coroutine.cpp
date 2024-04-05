#include <chrono>
#include "services/lua_service.h"
#include "common/time.hpp"

// #define DEBUG_LOG

static void
signal_hook(lua_State *L, lua_Debug *) {
	lua_service *S = lua_service::get(L);
	lua_sethook (L, nullptr, 0, 0);
	if (S->trap.load(std::memory_order_acquire)) {
        S->trap.store(0, std::memory_order_release);
		luaL_error(L, "signal 0");
	}
}

static void
switchL(lua_State *L, lua_service *S) {
	S->activeL = L;
	if (S->trap.load(std::memory_order_acquire)) {
		lua_sethook(L, signal_hook, LUA_MASKCOUNT, 1);
	}
}

static int
lua_resumeX(lua_State *L, lua_State *from, int nargs, int *nresults) {
	lua_service *S = lua_service::get(L);
	switchL(L, S);
	int err = lua_resume(L, from, nargs, nresults);
	if (S->trap.load(std::memory_order_acquire)) {
		// wait for lua_sethook. (l->trap == -1)
		while (S->trap.load(std::memory_order_acquire) >= 0) ;
	}
	switchL(from, S);
	return err;
}

static double
get_time() {
	return moon::time::clock();
}

static inline double
diff_time(double start) {
	double now = get_time();
	if (now < start) {
		return now + 0x10000 - start;
	} else {
		return now - start;
	}
}

// coroutine lib, add profile

/*
** Resumes a coroutine. Returns the number of results for non-error
** cases or -1 for errors.
*/
static int auxresume (lua_State *L, lua_State *co, int narg) {
  int status = 0;
  int nres = 0;
  if (!lua_checkstack(co, narg)) {
    lua_pushliteral(L, "too many arguments to resume");
    return -1;  /* error flag */
  }
  lua_xmove(L, co, narg);
  status = lua_resumeX(co, L, narg, &nres);
  if (status == LUA_OK || status == LUA_YIELD) {
    if (!lua_checkstack(L, nres + 1)) {
      lua_pop(co, nres);  /* remove results anyway */
      lua_pushliteral(L, "too many results to resume");
      return -1;  /* error flag */
    }
    lua_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    lua_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}

static int
timing_enable(lua_State *L, int co_index, lua_Number *start_time) {
	lua_pushvalue(L, co_index);
	lua_rawget(L, lua_upvalueindex(1));
	if (lua_isnil(L, -1)) {		// check total time
		lua_pop(L, 1);
		return 0;
	}
	*start_time = lua_tonumber(L, -1);
	lua_pop(L,1);
	return 1;
}

static double
timing_total(lua_State *L, int co_index) {
	lua_pushvalue(L, co_index);
	lua_rawget(L, lua_upvalueindex(2));
	double total_time = lua_tonumber(L, -1);
	lua_pop(L,1);
	return total_time;
}

static int
timing_resume(lua_State *L, int co_index, int n) {
	lua_State *co = lua_tothread(L, co_index);
	lua_Number start_time = 0;
	if (timing_enable(L, co_index, &start_time)) {
		start_time = get_time();
#ifdef DEBUG_LOG
		fprintf(stderr, "PROFILE [%p] resume %lf\n", co, ti);
#endif
		lua_pushvalue(L, co_index);
		lua_pushnumber(L, start_time);
		lua_rawset(L, lua_upvalueindex(1));	// set start time
	}

	int r = auxresume(L, co, n);

	if (timing_enable(L, co_index, &start_time)) {
		double total_time = timing_total(L, co_index);
		double diff = diff_time(start_time);
		total_time += diff;
#ifdef DEBUG_LOG
		fprintf(stderr, "PROFILE [%p] yield (%lf/%lf)\n", co, diff, total_time);
#endif
		lua_pushvalue(L, co_index);
		lua_pushnumber(L, total_time);
		lua_rawset(L, lua_upvalueindex(2));
	}

	return r;
}

static int luaB_coresume (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTHREAD);
  int r = timing_resume(L, 1, lua_gettop(L) - 1);
  if (r < 0) {
    lua_pushboolean(L, 0);
    lua_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    lua_pushboolean(L, 1);
    lua_insert(L, -(r + 1));
    return r + 1;  /* return true + 'resume' returns */
  }
}

static int luaB_auxwrap (lua_State *L) {
  lua_State *co = lua_tothread(L, lua_upvalueindex(3));
  int r = timing_resume(L, lua_upvalueindex(3), lua_gettop(L));
  if (r < 0) {
    int stat = lua_status(co);
    if (stat != LUA_OK && stat != LUA_YIELD)
      lua_closethread(co, L);  /* close variables in case of errors */
    if (lua_type(L, -1) == LUA_TSTRING) {  /* error object is a string? */
      luaL_where(L, 1);  /* add extra info, if available */
      lua_insert(L, -2);
      lua_concat(L, 2);
    }
    return lua_error(L);  /* propagate error */
  }
  return r;
}

static int luaB_cocreate (lua_State *L) {
  lua_State *NL;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  NL = lua_newthread(L);
  lua_pushvalue(L, 1);  /* move function to top */
  lua_xmove(L, NL, 1);  /* move function from L to NL */
  return 1;
}

static int luaB_cowrap (lua_State *L) {
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_pushvalue(L, lua_upvalueindex(2));
  luaB_cocreate(L);
  lua_pushcclosure(L, luaB_auxwrap, 3);
  return 1;
}

// profile lib

static int
lstart(lua_State *L) {
	if (lua_gettop(L) != 0) {
		lua_settop(L,1);
		luaL_checktype(L, 1, LUA_TTHREAD);
	} else {
		lua_pushthread(L);
	}
	lua_Number start_time = 0;
	if (timing_enable(L, 1, &start_time)) {
		return luaL_error(L, "Thread %p start profile more than once", lua_topointer(L, 1));
	}

	// reset total time
	lua_pushvalue(L, 1);
	lua_pushnumber(L, 0);
	lua_rawset(L, lua_upvalueindex(2));

	// set start time
	lua_pushvalue(L, 1);
	start_time = get_time();
#ifdef DEBUG_LOG
	fprintf(stderr, "PROFILE [%p] start\n", L);
#endif
	lua_pushnumber(L, start_time);
	lua_rawset(L, lua_upvalueindex(1));

	return 0;
}

static int
lstop(lua_State *L) {
	if (lua_gettop(L) != 0) {
		lua_settop(L,1);
		luaL_checktype(L, 1, LUA_TTHREAD);
	} else {
		lua_pushthread(L);
	}
	lua_Number start_time = 0;
	if (!timing_enable(L, 1, &start_time)) {
		return luaL_error(L, "Call profile.start() before profile.stop()");
	}
	double ti = diff_time(start_time);
	double total_time = timing_total(L,1);

	lua_pushvalue(L, 1);	// push coroutine
	lua_pushnil(L);
	lua_rawset(L, lua_upvalueindex(1));

	lua_pushvalue(L, 1);	// push coroutine
	lua_pushnil(L);
	lua_rawset(L, lua_upvalueindex(2));

	total_time += ti;
	lua_pushnumber(L, total_time);
#ifdef DEBUG_LOG
	fprintf(stderr, "PROFILE [%p] stop (%lf/%lf)\n", lua_tothread(L,1), ti, total_time);
#endif

	return 1;
}

extern "C" {
	int luaopen_coroutine_profile(lua_State *L) {
		luaL_Reg l[] = {
			{ "start", lstart },
			{ "stop", lstop },
			{ "resume", luaB_coresume },
			{ "wrap", luaB_cowrap },
			{ NULL, NULL },
		};

		luaL_newlibtable(L,l);//L [-0, +1]
		lua_newtable(L);	// table thread->start time L [-0, +1]
		lua_newtable(L);	// table thread->total time L [-0, +1]

		lua_newtable(L);	// weak table L [-0, +1]
		lua_pushliteral(L, "kv");
		lua_setfield(L, -2, "__mode");

		lua_pushvalue(L, -1);// L[-0, +1]
		lua_setmetatable(L, -3); //L[-0, -1]
		lua_setmetatable(L, -3);//L[-0, -1]

		luaL_setfuncs(L,l,2);

		// replace coroutine resume and wrap
		lua_getglobal(L, "coroutine");
		lua_getfield(L, 2, "resume");
		lua_setfield(L, -2, "resume");
		lua_getfield(L, 2, "warp");
		lua_setfield(L, -2, "wrap");
		lua_pop(L, 1);
		return 1;
	}
}