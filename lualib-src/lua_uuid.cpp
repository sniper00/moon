
#include "lua.hpp"
#include <cstdint>
#include <atomic>
#include <cassert>
#include <limits>
#include <array>

#define METANAME "luuid"

//////////////////////////////////uuid///////////////////////////////////
static constexpr int64_t FLAG_BITS = 1;
static constexpr int64_t TYPE_BITS = 9;
static constexpr int64_t SERVERID_BITS = 12;
static constexpr int64_t BOOTTIMES_BITS = 10;
static constexpr int64_t SEQUENCE_BITS = 31;

static constexpr int64_t TYPE_MAX =(int64_t{1}<< TYPE_BITS)-1;
static constexpr int64_t BOOTTIMES_MAX = (int64_t{1} << BOOTTIMES_BITS) - 1;
static constexpr int64_t SERVERID_MAX = (int64_t{1} << SERVERID_BITS) - 1;
static constexpr int64_t SEQUENCE_MAX = (int64_t{1} << SEQUENCE_BITS) - 1;

static constexpr int64_t TYPE_LEFT_SHIFT = SEQUENCE_BITS;
static constexpr int64_t BOOTTIMES_LEFT_SHIFT = TYPE_LEFT_SHIFT + TYPE_BITS;
static constexpr int64_t SERVERID_LEFT_SHIFT = BOOTTIMES_LEFT_SHIFT + BOOTTIMES_BITS;
static constexpr int64_t FLAG_LEFT_SHIFT = SERVERID_LEFT_SHIFT + SERVERID_BITS;

/////////////////////////////////player uid////////////////////////////////////
static constexpr int64_t UID_CHANNEL_BIT = 8;
static constexpr int64_t UID_SERVERID_BIT = 12;
static constexpr int64_t UID_BOOTTIMES_BIT = 10;
static constexpr int64_t UID_SEQUENCE_BIT = 18;

static constexpr int64_t UID_CHANNEL_MAX = (int64_t{1} << UID_CHANNEL_BIT)-1;
static constexpr int64_t UID_SERVERID_MAX = (int64_t{1} << UID_SERVERID_BIT) - 1;
static constexpr int64_t UID_BOOTTIMES_MAX = (int64_t{1} << UID_BOOTTIMES_BIT) -1;
static constexpr int64_t UID_SEQUENCE_MAX = (int64_t{1} << UID_SEQUENCE_BIT) - 1;

static constexpr int64_t UID_BOOTTIMES_LEFT_SHIFT = UID_SEQUENCE_BIT;
static constexpr int64_t UID_SERVERID_LEFT_SHIFT = UID_BOOTTIMES_LEFT_SHIFT + UID_BOOTTIMES_BIT;
static constexpr int64_t UID_CHANNEL_LEFT_SHIFT = UID_SERVERID_LEFT_SHIFT + UID_SERVERID_BIT;

struct uuid_state
{
    int64_t serverid = 0;
    int64_t boottimes = 0;
    int64_t channel = 0;
    std::array<std::atomic<int64_t>, TYPE_MAX+1> sequence;
};

static uuid_state g_uuid;

static int linit(lua_State *L)
{
    int64_t channel = (int64_t)luaL_checkinteger(L, 1);
    int64_t serverid = (int64_t)luaL_checkinteger(L, 2);
    int64_t boottimes = (int64_t)luaL_checkinteger(L, 3);

    luaL_argcheck(L, (channel > 0 && channel <= UID_CHANNEL_MAX), 1, "channel out off limit");
    luaL_argcheck(L, (serverid>0&& serverid<= SERVERID_MAX), 2, "serverid out off limit");
    luaL_argcheck(L, (boottimes > 0 && boottimes <= BOOTTIMES_MAX), 3, "boottimes out off limit");

    g_uuid.serverid = serverid;
    g_uuid.boottimes = boottimes;
    g_uuid.channel = channel;

    for (auto& v : g_uuid.sequence)
    {
        v = 1;
    }
    return 0;
}

static int lnext(lua_State *L)
{
    auto& u = g_uuid;
    if(u.serverid == 0 || u.boottimes == 0)
        return luaL_error(L, "not init"); 

    int64_t type = luaL_optinteger(L, 1, 0);
    luaL_argcheck(L, (type >=0 && type <= TYPE_MAX), 1, "type out off limit");

    if (type == 0)
    {
        auto sequence = u.sequence[type].fetch_add(1);;
        assert(u.serverid != 0 && u.boottimes != 0 && u.channel != 0);
        if (sequence > UID_SEQUENCE_MAX)
        {
            return luaL_error(L, "sequence out off limit");
        }

        //flag channel serverid bootimes sequence 
        //0    49-41   40-29    28-19    18-1
        int64_t v = (u.channel << UID_CHANNEL_LEFT_SHIFT) |
            (u.serverid << UID_SERVERID_LEFT_SHIFT) |
            (u.boottimes << UID_BOOTTIMES_LEFT_SHIFT) |
            (sequence);
        lua_pushinteger(L, v);
        return 1;
    }
    else
    {
        int64_t sequence = luaL_optinteger(L, 2, 0);
        if (sequence == 0)
        {
            sequence = u.sequence[type].fetch_add(1);
        }

        if (sequence > SEQUENCE_MAX)
        {
            return luaL_error(L, "luuid sequence out off limit");
        }

        //flag serverid boottimes  type    sequence
        //63   62-51    50-41      40-31   30-1
        int64_t v = (int64_t{ 1 } << FLAG_LEFT_SHIFT) |
            (u.serverid << SERVERID_LEFT_SHIFT) |
            (u.boottimes << BOOTTIMES_LEFT_SHIFT ) |
            (type << TYPE_LEFT_SHIFT) |
            (sequence);
        lua_pushinteger(L, v);
        return 1;
    }
}

static int ltype(lua_State *L)
{
    int64_t uuid = (int64_t)luaL_checkinteger(L, 1);
    if ((uuid>> FLAG_LEFT_SHIFT) != 1)
    {
        return luaL_error(L, "attemp get type from a not uuid value");
    }
    lua_pushinteger(L, (uuid>> TYPE_LEFT_SHIFT)&TYPE_MAX);
    return 1;
}

static int isuid(lua_State* L)
{
    int64_t uuid = (int64_t)luaL_checkinteger(L, 1);

    do {
        if ((uuid >> FLAG_LEFT_SHIFT) != 0)
            break;
        if (!(uuid & UID_SEQUENCE_MAX))
            break;
        if (!((uuid >> UID_BOOTTIMES_LEFT_SHIFT) & UID_BOOTTIMES_MAX))
            break;
        if (!((uuid >> UID_SERVERID_LEFT_SHIFT) & UID_SERVERID_MAX))
            break;
        if (!((uuid >> UID_CHANNEL_LEFT_SHIFT) & UID_CHANNEL_MAX))
            break;
        lua_pushboolean(L, true);
        return 1;
    } while (false);
    lua_pushboolean(L, false);
    return 1;
}

static int serverid(lua_State* L)
{
    int64_t uuid = (int64_t)luaL_checkinteger(L, 1);
    if ((uuid >> FLAG_LEFT_SHIFT) != 0)
    {
        lua_pushinteger(L, (uuid >> SERVERID_LEFT_SHIFT) & SERVERID_MAX);
        return 1;
    }
    else
    {
        lua_pushinteger(L, (uuid >> UID_SERVERID_LEFT_SHIFT) & UID_SERVERID_MAX);
        return 1;
    }
}

extern "C"
{
    int LUAMOD_API luaopen_uuid(lua_State *L)
    {
        luaL_Reg l[] = {
            {"init",linit},
            {"next",lnext },
            {"type",ltype },
            {"isuid",isuid },
            {"serverid", serverid},
            {NULL,NULL}
        };
        luaL_checkversion(L);
        luaL_newlib(L, l);
        return 1;
    }
}
