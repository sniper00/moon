
#include "lua.hpp"
#include <random>
#include <cassert>
#include <cstdint>
#include <numeric>
#include "common/lua_utility.hpp"

struct thread_rand_context
{
    std::vector<int64_t>& get_w() {w.clear(); return w;}
    std::vector<int64_t>& get_v() {v.clear(); return v;}
    std::mt19937 generator{std::random_device{}()};
private:
    std::vector<int64_t> w;
    std::vector<int64_t> v;
};

static thread_local thread_rand_context thread_rng;

inline unsigned int rand_u32()
{
    std::uniform_int_distribution<unsigned int> dis(1, std::numeric_limits<unsigned int>::max());
    return dis(thread_rng.generator);
}

///[min.max]
template<typename IntType>
IntType rand_range(IntType min, IntType max)
{
    std::uniform_int_distribution<IntType> dis(min, max);
    return dis(thread_rng.generator);
}

///[min,max)
template< class RealType = double >
RealType randf_range(RealType min, RealType max)
{
    std::uniform_real_distribution<RealType> dis(min, max);
    return dis(thread_rng.generator);
}

template< class RealType = double >
bool randf_percent(RealType percent)
{
    if (percent <= 0.0)
    {
        return false;
    }
    if (randf_range(0.0f,1.0f) >= percent)
    {
        return false;
    }
    return true;
}

template<typename Values, typename Weights>
auto rand_weight(const Values& v, const Weights& w)
{
    if (v.empty() || v.size() != w.size())
    {
        return -1;
    }

    auto dist = std::discrete_distribution<int>(w.begin(), w.end());
    int index = dist(thread_rng.generator);
    return v[index];
}


//[min,max]
static int lrand_range(lua_State* L)
{
    int64_t v_min = (int64_t)luaL_checkinteger(L, 1);
    int64_t v_max = (int64_t)luaL_checkinteger(L, 2);
    if (v_min > v_max)
    {
        return luaL_error(L, "argument error: #1 > #2");
    }
    auto res = rand_range(v_min, v_max);
    lua_pushinteger(L, res);
    return 1;
}

//[min,max]
static int lrand_range_some(lua_State* L)
{
    int64_t v_min = (int64_t)luaL_checkinteger(L, 1);
    int64_t v_max = (int64_t)luaL_checkinteger(L, 2);
    int64_t v_count = (int64_t)luaL_checkinteger(L, 3);

    if (v_count <= 0 || (v_max - v_min + 1) < v_count)
    {
        return luaL_error(L, "rand_range_some range count < num");
    }

    thread_rng.get_v().resize(v_max - v_min + 1, 0);

    std::vector<int64_t>& vec = thread_rng.get_v();
    vec.resize(v_max - v_min + 1);
    std::iota(vec.begin(), vec.end(), v_min);
    lua_createtable(L, (int)v_count, 0);
    int count = 0;
    while (v_count > 0)
    {
        auto index = rand_range((size_t)0, vec.size() - 1);
        lua_pushinteger(L, vec[index]);
        lua_rawseti(L, -2, ++count);
        vec[index] = vec[vec.size() - 1];
        vec.pop_back();
        --v_count;
    }
    return 1;
}

//[min,max)
static int lrandf_range(lua_State* L)
{
    double v_min = (double)luaL_checknumber(L, 1);
    double v_max = (double)luaL_checknumber(L, 2);
    auto res = randf_range(v_min, v_max);
    lua_pushnumber(L, res);
    return 1;
}

static int lrandf_percent(lua_State* L)
{
    double v = (double)luaL_checknumber(L, 1);
    auto res = randf_percent(v);
    lua_pushboolean(L, res);
    return 1;
}

static int lrand_weight(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TTABLE);

    auto vlen = lua_rawlen(L, 1);
    auto wlen = lua_rawlen(L, 2);

    if (vlen != wlen || vlen == 0)
    {
        return luaL_error(L, "lrand_weight table empty or values size != weights size");
    }

    moon::lua_array_view<int64_t> values{L, 1};
    moon::lua_array_view<int64_t> weights{L, 2};

    int64_t sum = std::accumulate(weights.begin(), weights.end(), int64_t{0});
    if (sum == 0)
    {
        return 0;
    }

    int64_t cutoff = rand_range(int64_t{0}, sum - 1);
    auto vi = values.begin();
    auto wi = weights.begin();
    while (cutoff >= *wi)
    {
        cutoff -= *wi++;
        ++vi;
    }
    lua_pushinteger(L, *vi);
    return 1;
}

static int lrand_weight_some(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TTABLE);
    int64_t count = luaL_checkinteger(L, 3);

    auto& w = thread_rng.get_w();
    auto& v = thread_rng.get_v();

    moon::table_to_vector<int64_t>(L, 1, v);
    moon::table_to_vector<int64_t>(L, 2, w);
    if (v.size() != w.size() || v.size() == 0 || count < 0 || (int64_t)v.size() < count)
    {
        return luaL_error(L, "lrand_weight_some table empty or values size != weights size");
    }

    lua_createtable(L, (int)count, 0);
    for (int64_t i = 0; i < count; ++i)
    {
        int64_t sum = std::accumulate(w.begin(), w.end(), int64_t{0});
        if (sum == 0)
        {
            lua_pop(L, 1); // pop table
            return 0;
        }
        int64_t cutoff = rand_range(int64_t{0}, sum - 1);
        auto idx = 0;
        while (cutoff >= w[idx])
        {
            cutoff -= w[idx];
            ++idx;
        }
        lua_pushinteger(L, v[idx]);
        lua_rawseti(L, -2, i + 1);
        v[idx] = v[v.size() - 1];
        v.pop_back();
        w[idx] = w[w.size() - 1];
        w.pop_back();
    }
    return 1;
}

extern "C" {
    int LUAMOD_API luaopen_random(lua_State* L)
    {
        luaL_Reg l[] = {
            {"rand_range",lrand_range},
            {"rand_range_some",lrand_range_some},
            {"randf_range",lrandf_range },
            {"randf_percent",lrandf_percent },
            {"rand_weight",lrand_weight },
            {"rand_weight_some",lrand_weight_some },
            {NULL,NULL}
        };
        luaL_newlib(L, l);
        return 1;
    }
}
