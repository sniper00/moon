#include "common/crypto/scram_sha256.hpp"
#include "lua.hpp"

#define METANAME "lscram"

using moon::crypto::scram::ScramSha256Client;

static ScramSha256Client* get_client(lua_State* L, int index) {
    int tp = lua_type(L, index);
    if (tp != LUA_TUSERDATA) {
        luaL_error(
            L,
            "bad argument #%d to 'crypto_scram' (crypto_scram: expected scram client userdata, got %s)",
            index,
            lua_typename(L, tp)
        );
        return nullptr;
    }
    auto* client = static_cast<ScramSha256Client*>(lua_touserdata(L, index));
    if (client == nullptr) {
        luaL_argerror(L, index, "crypto_scram: expected scram client userdata, got null pointer");
        return nullptr;
    }
    return client;
}

// Lua binding for prepare_first_message() -> string
static int lprepare_first(lua_State* L) {
    auto* client = get_client(L, 1);

    try {
        std::string msg = client->prepare_first_message();
        lua_pushlstring(L, msg.data(), msg.size());
        return 1; // Return the message string
    } catch (const std::exception& e) {
        lua_pushfstring(L, "crypto_scram.prepare_first_message: %s", e.what());
    }
    return lua_error(L);
}

// Lua binding for process_server_first(server_first_message)
static int lprocess_server_first(lua_State* L) {
    auto* client = get_client(L, 1);

    size_t msg_len;
    const char* msg = luaL_checklstring(L, 2, &msg_len);
    try {
        client->process_server_first(std::string(msg, msg_len));
        return 0; // No return value
    } catch (const std::exception& e) {
        lua_pushfstring(L, "crypto_scram.process_server_first: %s", e.what());
    }
    return lua_error(L);
}

// Lua binding for prepare_final_message() -> string
static int lprepare_final(lua_State* L) {
    auto* client = get_client(L, 1);

    try {
        std::string msg = client->prepare_final_message();
        lua_pushlstring(L, msg.data(), msg.size());
        return 1; // Return the message string
    } catch (const std::exception& e) {
        lua_pushfstring(L, "crypto_scram.prepare_final_message: %s", e.what());
    }
    return lua_error(L);
}

// Lua binding for process_server_final(server_final_message)
static int lprocess_server_final(lua_State* L) {
    auto* client = get_client(L, 1);

    size_t msg_len;
    const char* msg = luaL_checklstring(L, 2, &msg_len);
    try {
        client->process_server_final(std::string(msg, msg_len));
        return 0; // No return value
    } catch (const std::exception& e) {
        lua_pushfstring(L, "crypto_scram.process_server_final: %s", e.what());
    }
    return lua_error(L);
}

// Lua binding for is_authenticated() -> boolean
static int lis_authenticated(lua_State* L) {
    auto* client = get_client(L, 1);
    lua_pushboolean(L, client->is_authenticated());
    return 1;
}

// Lua binding for is_error() -> boolean
static int lis_error(lua_State* L) {
    auto* client = get_client(L, 1);
    lua_pushboolean(L, client->is_error());
    return 1;
}

// Lua binding for get_state() -> integer
static int lget_state(lua_State* L) {
    auto* client = get_client(L, 1);
    // Push the enum value as an integer
    lua_pushinteger(L, static_cast<lua_Integer>(client->get_state()));
    return 1;
}

// Lua binding for ScramSha256Client destructor (__gc metamethod)
static int lrelease(lua_State* L) {
    auto* client = get_client(L, 1);
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
        return luaL_error(L, "crypto_scram.new: failed to allocate userdata for scram client");
    }

    try {
        // Construct the object directly in the userdata memory (placement new)
        new (p) ScramSha256Client(std::string(username, user_len), std::string(password, pass_len));
    } catch (const std::exception& e) {
        // No need to manually free userdata memory here, Lua GC handles it
        lua_pushfstring(L, "crypto_scram.new: %s", e.what());
        return lua_error(L); // Propagate error to Lua
    } catch (...) {
        lua_pushstring(L, "crypto_scram.new: unknown error creating scram client");
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
            { NULL, NULL },
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
LUAMOD_API int luaopen_crypto_scram(lua_State* L) {
    luaL_Reg l[] = {
        { "new", lnew },
        { nullptr, nullptr },
    };
    luaL_newlib(L, l);
    return 1; // Return the module table
}

