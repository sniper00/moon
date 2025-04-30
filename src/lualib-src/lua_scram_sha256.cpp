#include "lua.hpp"
#include "common/crypto/scram_sha256.hpp"

#define METANAME "lscram"

using moon::crypto::scram::ScramSha256Client;

// Lua binding for prepare_first_message() -> string
static int lprepare_first(lua_State* L) {
    ScramSha256Client* client = (ScramSha256Client*)lua_touserdata(L, 1);
    if (nullptr == client)
        return luaL_argerror(L, 1, "invalid scram client pointer");

    try {
        std::string msg = client->prepare_first_message();
        lua_pushlstring(L, msg.data(), msg.size());
        return 1; // Return the message string
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

// Lua binding for process_server_first(server_first_message)
static int lprocess_server_first(lua_State* L) {
    ScramSha256Client* client = (ScramSha256Client*)lua_touserdata(L, 1);
     if (nullptr == client)
        return luaL_argerror(L, 1, "invalid scram client pointer");

    size_t msg_len;
    const char* msg = luaL_checklstring(L, 2, &msg_len);
    try {
        client->process_server_first(std::string(msg, msg_len));
        return 0; // No return value
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

// Lua binding for prepare_final_message() -> string
static int lprepare_final(lua_State* L) {
    ScramSha256Client* client = (ScramSha256Client*)lua_touserdata(L, 1);
     if (nullptr == client)
        return luaL_argerror(L, 1, "invalid scram client pointer");

    try {
        std::string msg = client->prepare_final_message();
        lua_pushlstring(L, msg.data(), msg.size());
        return 1; // Return the message string
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

// Lua binding for process_server_final(server_final_message)
static int lprocess_server_final(lua_State* L) {
    ScramSha256Client* client = (ScramSha256Client*)lua_touserdata(L, 1);
     if (nullptr == client)
        return luaL_argerror(L, 1, "invalid scram client pointer");

    size_t msg_len;
    const char* msg = luaL_checklstring(L, 2, &msg_len);
    try {
        client->process_server_final(std::string(msg, msg_len));
        return 0; // No return value
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

// Lua binding for is_authenticated() -> boolean
static int lis_authenticated(lua_State* L) {
    ScramSha256Client* client = (ScramSha256Client*)lua_touserdata(L, 1);
     if (nullptr == client)
        return luaL_argerror(L, 1, "invalid scram client pointer");
    lua_pushboolean(L, client->is_authenticated());
    return 1;
}

// Lua binding for is_error() -> boolean
static int lis_error(lua_State* L) {
    ScramSha256Client* client = (ScramSha256Client*)lua_touserdata(L, 1);
     if (nullptr == client)
        return luaL_argerror(L, 1, "invalid scram client pointer");
    lua_pushboolean(L, client->is_error());
    return 1;
}

// Lua binding for get_state() -> integer
static int lget_state(lua_State* L) {
    ScramSha256Client* client = (ScramSha256Client*)lua_touserdata(L, 1);
     if (nullptr == client)
        return luaL_argerror(L, 1, "invalid scram client pointer");
    // Push the enum value as an integer
    lua_pushinteger(L, static_cast<lua_Integer>(client->get_state()));
    return 1;
}

// Lua binding for ScramSha256Client destructor (__gc metamethod)
static int lrelease(lua_State* L) {
    ScramSha256Client* client = (ScramSha256Client*)lua_touserdata(L, 1);
    if (nullptr == client)
        return luaL_argerror(L, 1, "invalid scram client pointer");
    std::destroy_at(client);
    return 0;
}

// Lua binding for ScramSha256Client constructor: scram.new(username, password)
static int lnew(lua_State* L) {
    size_t user_len, pass_len;
    const char* username = luaL_checklstring(L, 1, &user_len);
    const char* password = luaL_checklstring(L, 2, &pass_len);

    // Allocate memory within Lua for the C++ object itself
    void* p = lua_newuserdatauv(L, sizeof(ScramSha256Client), 0);
    if (!p) {
         return luaL_error(L, "Failed to allocate userdata for ScramSha256Client");
    }

    try {
        // Construct the object directly in the userdata memory (placement new)
        new (p) ScramSha256Client(std::string(username, user_len), std::string(password, pass_len));
    } catch (const std::exception& e) {
        // No need to manually free userdata memory here, Lua GC handles it
        lua_pushstring(L, e.what());
        return lua_error(L); // Propagate error to Lua
    } catch (...) {
        lua_pushstring(L, "Unknown error creating ScramSha256Client");
        return lua_error(L); // Propagate error to Lua
    }

    // Associate the metatable with the userdata
    if (luaL_newmetatable(L, METANAME)) //mt
    {
        luaL_Reg l[] = {
            { "prepare_first_message", lprepare_first },
            { "process_server_first", lprocess_server_first },
            { "prepare_final_message", lprepare_final },
            { "process_server_final", lprocess_server_final },
            { "is_authenticated", lis_authenticated },
            { "is_error", lis_error },
            { "get_state", lget_state },
            { NULL, NULL }
        };
        luaL_newlib(L, l); //{}
        lua_setfield(L, -2, "__index"); //mt[__index] = {}
        lua_pushcfunction(L, lrelease);
        lua_setfield(L, -2, "__gc"); //mt[__gc] = lrelease
    }
    lua_setmetatable(L, -2); // set userdata metatable
    return 1; // Return the userdata object to Lua
}


// Module entry point function
extern "C" {
    int LUAMOD_API luaopen_crypto_scram(lua_State* L) {
        luaL_Reg l[] = {
            { "new", lnew },
            { nullptr, nullptr }
        };
        luaL_newlib(L, l);
        return 1; // Return the module table
    }
}
