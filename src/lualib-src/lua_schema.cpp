#include "common/hash.hpp"
#include "common/lua_utility.hpp"
#include "common/string.hpp"
#include "lua.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>

using namespace moon;

enum class compose_type { T_NONE, T_ARRAY, T_OBJECT };

struct proto_field {
    compose_type container_type = compose_type::T_NONE;
    std::string key_type;
    std::string value_type;
    uint32_t value_index = 0;
};

static std::unordered_map<std::string, std::unordered_map<std::string, proto_field>> schema_define;

class lua_schema_error: public std::runtime_error {
public:
    using runtime_error::runtime_error;
};

static inline size_t array_size(lua_State* L, int index) {
    auto len = (lua_Integer)lua_rawlen(L, index);
    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        if (lua_isinteger(L, -2)) {
            lua_Integer x = lua_tointeger(L, -2);
            if (x > 0 && x <= len) {
                lua_pop(L, 1);
                continue;
            }
        }
        lua_pop(L, 2);
        return std::numeric_limits<size_t>::max();
    }
    return len;
}

static int type_check(lua_State* L, int index, const std::string_view& type) {
    switch (moon::chash_string(type)) {
        case "int32"_csh:
        case "uint32"_csh:
        case "int64"_csh:
        case "uint64"_csh:
        case "sint32"_csh:
        case "sint64"_csh:
        case "fixed32"_csh:
        case "fixed64"_csh:
        case "sfixed32"_csh:
        case "sfixed64"_csh: {
            return lua_isinteger(L, index) ? 1 : 0;
        }
        case "bool"_csh: {
            return lua_type(L, index) == LUA_TBOOLEAN;
        }
        case "float"_csh:
        case "double"_csh: {
            return lua_type(L, index) == LUA_TNUMBER;
        }
        case "string"_csh:
        case "bytes"_csh: {
            return lua_type(L, index) == LUA_TSTRING;
        }
        default:
            return -1;
    }
}

static std::string trace_to_string(const std::vector<std::string>& trace) {
    std::string res;
    for (const auto& v: trace) {
        if (!res.empty())
            res.append(".");
        res.append(v);
    }
    return res;
}

static void do_verify(
    lua_State* L,
    const std::string_view& proto_name,
    int index,
    std::vector<std::string>& trace
);

static void check_field_type(
    lua_State* L,
    int index,
    const std::string_view& proto_name,
    const std::string_view& field_name,
    const std::string_view& name,
    const std::string_view& type,
    std::vector<std::string>& trace
) {
    index = lua_absindex(L, index);
    int ok = type_check(L, index, type);
    if (ok < 0) {
        do_verify(L, type, index, trace);
    } else if (0 == ok) {
        std::string value = luaL_tolstring(L, index, nullptr);
        lua_pop(L, 1);
        if (name.empty()) {
            throw lua_schema_error { moon::format(
                "'%s.%s' %s expected, got %s, value '%s'. trace: %s",
                proto_name.data(),
                field_name.data(),
                type.data(),
                luaL_typename(L, index),
                value.data(),
                trace_to_string(trace).data()
            ) };
        } else {
            throw lua_schema_error { moon::format(
                "'%s.%s.%s' %s expected, got %s. trace: %s",
                proto_name.data(),
                field_name.data(),
                name.data(),
                type.data(),
                luaL_typename(L, index),
                trace_to_string(trace).data()
            ) };
        }
    }
}

static void verify_field(
    lua_State* L,
    int index,
    const std::string_view& proto_name,
    const std::unordered_map<std::string, proto_field>& proto,
    const char* field_name,
    std::vector<std::string>& trace
) {
    index = lua_absindex(L, index);

    trace.emplace_back(field_name);

    if (auto field_iter = proto.find(field_name); field_iter == proto.end()) {
        throw lua_schema_error { moon::format(
            "Attemp to index undefined field: '%s.%s'. trace: %s",
            proto_name.data(),
            field_name,
            trace_to_string(trace).data()
        ) };
    } else {
        const auto& field = field_iter->second;
        switch (field.container_type) {
            case compose_type::T_ARRAY: {
                if (lua_type(L, index) != LUA_TTABLE) {
                    throw lua_schema_error { moon::format(
                        "'%s.%s' table expected, got %s. trace: %s",
                        proto_name.data(),
                        field_name,
                        luaL_typename(L, index),
                        trace_to_string(trace).data()
                    ) };
                }

                auto size = array_size(L, index);
                if (size == std::numeric_limits<size_t>::max()) {
                    throw lua_schema_error { moon::format(
                        "'%s.%s' not meet lua array requirements. trace: %s",
                        proto_name.data(),
                        field_name,
                        trace_to_string(trace).data()
                    ) };
                }

                for (size_t i = 1; i <= size; i++) {
                    trace.push_back(std::to_string(i));
                    lua_rawgeti(L, index, i);
                    check_field_type(
                        L,
                        -1,
                        proto_name,
                        field_name,
                        std::to_string(i),
                        field.value_type,
                        trace
                    );
                    lua_pop(L, 1);
                    trace.pop_back();
                }
                break;
            }
            case compose_type::T_OBJECT: {
                if (lua_type(L, index) != LUA_TTABLE) {
                    throw lua_schema_error { moon::format(
                        "'%s.%s' table expected, got %s. trace: %s",
                        proto_name.data(),
                        field_name,
                        luaL_typename(L, index),
                        trace_to_string(trace).data()
                    ) };
                }
                lua_pushnil(L);
                std::string key_value;
                while (lua_next(L, index) != 0) {
                    if (lua_isinteger(L, -2) && lua_tointeger(L, -2) == 1) {
                        if (luaL_getmetafield(L, index, "__object") == LUA_TNIL)
                        {
                            throw lua_schema_error { moon::format(
                                "'%s.%s': table of type 'object' uses integer key=1, but missing required metafield '__object'. Trace: %s",
                                proto_name.data(),
                                field_name,
                                trace_to_string(trace).data()
                            ) };
                        }
                        lua_pop(L, 1); // pop metafield
                    }
                    key_value = luaL_tolstring(L, -2, nullptr);
                    lua_pop(L, 1);
                    trace.push_back(key_value);
                    check_field_type(L, -2, proto_name, field_name, "$key", field.key_type, trace);
                    check_field_type(
                        L,
                        -1,
                        proto_name,
                        field_name,
                        key_value,
                        field.value_type,
                        trace
                    );
                    trace.pop_back();
                    lua_pop(L, 1);
                }
                break;
            }
            default: {
                check_field_type(L, index, proto_name, field_name, "", field.value_type, trace);
                break;
            }
        }
    }
    trace.pop_back();
}

static void do_verify(
    lua_State* L,
    const std::string_view& proto_name,
    int index,
    std::vector<std::string>& trace
) {
    luaL_checkstack(L, LUA_MINSTACK, nullptr);

    index = lua_absindex(L, index);

    if (!lua_istable(L, index)) {
        throw lua_schema_error { moon::format(
            "'%s' table expected, got %s. trace: %s",
            proto_name.data(),
            luaL_typename(L, index),
            trace_to_string(trace).data()
        ) };
    }

    auto iter = schema_define.find(std::string { proto_name });
    if (iter == schema_define.end()) {
        throw lua_schema_error { moon::format(
            "Attemp using undefined proto: %s. trace: %s",
            proto_name.data(),
            trace_to_string(trace).data()
        ) };
    }

    const auto& proto = iter->second;

    if (proto_name.substr(0, 6) == "array_" || proto_name.substr(0, 4) == "map_") {
        verify_field(L, -1, proto_name, proto, "data", trace);
    } else {
        lua_pushnil(L);
        while (lua_next(L, index) != 0) {
            const char* key = luaL_checkstring(L, -2);
            verify_field(L, -1, proto_name, proto, key, trace);
            lua_pop(L, 1);
        }
    }
}

static int validate(lua_State* L) {
    auto proto_name = moon::lua_check<std::string_view>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_settop(L, 2);
    try {
        std::vector<std::string> trace;
        trace.emplace_back(proto_name);
        do_verify(L, proto_name, 2, trace);
        return 0;
    } catch (const lua_schema_error& ex) {
        lua_pushstring(L, ex.what());
    }
    return lua_error(L);
}

static int load(lua_State* L) {
    if (!schema_define.empty())
        return luaL_error(L, "Schema can only be loaded once");

    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 1);
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        luaL_checktype(L, -1, LUA_TTABLE);

        std::string proto_name = luaL_checkstring(L, -2);
        std::unordered_map<std::string, proto_field> one;

        lua_pushnil(L); //+1
        while (lua_next(L, -2) != 0) {
            proto_field field;
            std::string field_name = luaL_checkstring(L, -2);
            luaL_checktype(L, -1, LUA_TTABLE);
            if (auto container = moon::lua_opt_field<std::string_view>(L, -1, "container", "");
                container == "array")
            {
                field.container_type = compose_type::T_ARRAY;
            } else if (container == "object") {
                field.container_type = compose_type::T_OBJECT;
            } else {
                field.container_type = compose_type::T_NONE;
            }

            field.key_type = moon::lua_opt_field<std::string_view>(L, -1, "key_type", "");
            field.value_type = moon::lua_opt_field<std::string_view>(L, -1, "value_type", "");
            lua_pop(L, 1);

            one.emplace(field_name, std::move(field));
        }
        lua_pop(L, 1);

        schema_define.emplace(proto_name, std::move(one));
    }
    return 0;
}

extern "C" {
int LUAMOD_API luaopen_schema(lua_State* L) {
    luaL_Reg l[] = {
        { "load", load },
        { "validate", validate },
        { NULL, NULL },
    };
    luaL_newlib(L, l);
    return 1;
}
}
