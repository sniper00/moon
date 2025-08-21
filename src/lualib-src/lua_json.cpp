#include "common/buffer.hpp"
#include "common/hash.hpp"
#include "common/lua_utility.hpp"
#include "common/string.hpp"
#include "lua.hpp"
#include "yyjson/yyjson.c"
#include <charconv>
#include <codecvt>
#include <cstdarg>
#include <cstdlib>
#include <string_view>

using namespace moon;

static void* json_malloc(void*, size_t size) {
#ifdef MOON_ENABLE_MIMALLOC
    return mi_malloc(size);
#else
    return std::malloc(size);
#endif
}

static void* json_realloc(void*, void* ptr, size_t, size_t size) {
#ifdef MOON_ENABLE_MIMALLOC
    return mi_realloc(ptr, size);
#else
    return std::realloc(ptr, size);
#endif
}

static void json_free(void*, void* ptr) {
#ifdef MOON_ENABLE_MIMALLOC
    mi_free(ptr);
#else
    std::free(ptr);
#endif
}
static const yyjson_alc allocator = { json_malloc, json_realloc, json_free, nullptr };

using namespace std::literals::string_view_literals;
using namespace moon;

static constexpr std::string_view json_null = "null"sv;
static constexpr std::array<std::string_view, 2> bool_string = { "false"sv, "true"sv };
static constexpr std::string_view JSON_OBJECT_MT = "MOON_JSON_OBJECT"sv;
static constexpr std::string_view JSON_ARRAY_MT = "MOON_JSON_ARRAY"sv;

static constexpr int MAX_DEPTH = 64;

static constexpr size_t DEFAULT_CONCAT_BUFFER_SIZE = 512;

static const std::array<char, 256> char2escape = {
    'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'b', 't', 'n', 'u', 'f',  'r', 'u', 'u',
    'u', 'u', 'u', 'u', // 0~19
    'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 0,    0,   '"', 0,
    0,   0,   0,   0, // 20~39
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,
    0,   0,   0,   0, // 40~59
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,
    0,   0,   0,   0, // 60~79
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   '\\', 0,   0,   0,
    0,   0,   0,   0, // 80~99
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,
    0,   0,   0,   0, // 100~119
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,
    0,   0,   0,   0, // 120~139
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,
    0,   0,   0,   0, // 140~159
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,
    0,   0,   0,   0, // 160~179
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,
    0,   0,   0,   0, // 180~199
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,
    0,   0,   0,   0, // 200~219
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,
    0,   0,   0,   0, // 220~239
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0, // 240~256
};

static const std::array<char, 16> hex_digits = { '0', '1', '2', '3', '4', '5', '6', '7',
                                                 '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

class lua_json_error: public std::runtime_error {
public:
    using runtime_error::runtime_error;

    static lua_json_error format(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        return lua_json_error(buffer);
    }
};

static buffer* get_thread_encode_buffer() {
    static thread_local buffer thread_encode_buffer { DEFAULT_CONCAT_BUFFER_SIZE };
    thread_encode_buffer.clear();
    return &thread_encode_buffer;
}

struct json_config {
    bool empty_as_array = true;
    bool enable_number_key = true;
    bool enable_sparse_array = false;
    bool has_metatfield = true;
    size_t concat_buffer_size = DEFAULT_CONCAT_BUFFER_SIZE;
};

static int json_destroy_config(lua_State* L) {
    if (auto* cfg = (json_config*)lua_touserdata(L, 1))
        std::destroy_at(cfg);
    return 0;
}

static void json_create_config(lua_State* L) {
    void* mem = lua_newuserdatauv(L, sizeof(json_config), 0);
    new (mem) json_config {};
    lua_newtable(L);
    lua_pushcfunction(L, json_destroy_config);
    lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -2);
}

static json_config* json_fetch_config(lua_State* L) {
    auto* cfg = (json_config*)lua_touserdata(L, lua_upvalueindex(1));
    if (!cfg)
        luaL_error(L, "Unable to fetch json configuration");
    return cfg;
}

static int json_options(lua_State* L) {
    json_config* cfg = json_fetch_config(L);
    size_t len = 0;
    const char* name = luaL_checklstring(L, 1, &len);
    int top = lua_gettop(L);
    switch (moon::chash_string(name, len)) {
        case "encode_empty_as_array"_csh: {
            bool v = cfg->empty_as_array;
            cfg->empty_as_array = static_cast<bool>(lua_toboolean(L, 2));
            lua_pushboolean(L, v ? 1 : 0);
            break;
        }
        case "enable_number_key"_csh: {
            bool v = cfg->enable_number_key;
            cfg->enable_number_key = static_cast<bool>(lua_toboolean(L, 2));
            lua_pushboolean(L, v ? 1 : 0);
            break;
        }
        case "enable_sparse_array"_csh: {
            bool v = cfg->enable_sparse_array;
            cfg->enable_sparse_array = static_cast<bool>(lua_toboolean(L, 2));
            lua_pushboolean(L, v ? 1 : 0);
            break;
        }
        case "has_metatfield"_csh: {
            bool v = cfg->has_metatfield;
            cfg->has_metatfield = static_cast<bool>(lua_toboolean(L, 2));
            lua_pushboolean(L, v ? 1 : 0);
            break;
        }
        case "concat_buffer_size"_csh: {
            auto new_size = static_cast<uint32_t>(luaL_checkinteger(L, 2));
            luaL_argcheck(L, new_size >= BUFFER_OPTION_CHEAP_PREPEND, 2, "buffer size too small");
            lua_pushinteger(
                L,
                static_cast<lua_Integer>(std::exchange(cfg->concat_buffer_size, new_size))
            );
            break;
        }
        default:
            return luaL_error(L, "Invalid json.options '%s'", name);
    }
    return lua_gettop(L) - top;
}

template<bool format>
static void format_new_line(buffer* writer) {
    if constexpr (format) {
        writer->write_back('\n');
    }
}

template<bool format>
static void format_space(buffer* writer, int n) {
    if constexpr (format) {
        writer->prepare(2 * size_t(n));
        for (int i = 0; i < n; ++i) {
            writer->unsafe_write_back(' ');
            writer->unsafe_write_back(' ');
        }
    }
}

inline void write_number(buffer* writer, lua_Number num) {
    auto [buf, n] = writer->prepare(32);
    uint64_t raw = f64_to_raw(num);
    auto e = write_f64_raw((uint8_t*)buf, raw, YYJSON_WRITE_ALLOW_INF_AND_NAN);
    writer->commit_unchecked(e - (uint8_t*)buf);
}

template<bool format>
static void encode_table(lua_State* L, buffer* writer, int idx, int depth);

template<bool format>
static void encode_one(lua_State* L, buffer* writer, int idx, int depth, const json_config* cfg) {
    int t = lua_type(L, idx);
    switch (t) {
        case LUA_TBOOLEAN: {
            writer->write_back(bool_string[lua_toboolean(L, idx)]);
            return;
        }
        case LUA_TNUMBER: {
            if (lua_isinteger(L, idx))
                writer->write_chars(lua_tointeger(L, idx));
            else
                write_number(writer, lua_tonumber(L, idx));
            return;
        }
        case LUA_TSTRING: {
            size_t len = 0;
            const char* str = lua_tolstring(L, idx, &len);
            writer->prepare(len * 6 + 2);
            writer->unsafe_write_back('\"');
            for (size_t i = 0; i < len; ++i) {
                auto ch = (unsigned char)str[i];
                char esc = char2escape[ch];
                if (!esc) {
                    writer->unsafe_write_back((char)ch);
                } else {
                    writer->unsafe_write_back('\\');
                    writer->unsafe_write_back(esc);
                    if (esc == 'u') {
                        writer->unsafe_write_back('0');
                        writer->unsafe_write_back('0');
                        writer->unsafe_write_back(hex_digits[ch >> 4]);
                        writer->unsafe_write_back(hex_digits[ch & 0xF]);
                    }
                }
            }
            writer->unsafe_write_back('\"');
            return;
        }
        case LUA_TTABLE: {
            encode_table<format>(L, writer, idx, depth, cfg);
            return;
        }
        case LUA_TNIL: {
            writer->write_back(json_null);
            return;
        }
        case LUA_TLIGHTUSERDATA: {
            if (lua_touserdata(L, idx) == nullptr) {
                writer->write_back(json_null);
                return;
            }
            break;
        }
        default:
            throw lua_json_error::format(
                "json encode: unsupported value type: %s",
                lua_typename(L, t)
            );
    }
}

static inline std::pair<bool, size_t> is_array(lua_State* L, int index, const json_config* cfg) {
    if (cfg->has_metatfield) {
        if (luaL_getmetafield(L, index, "__array") != LUA_TNIL) {
            lua_pop(L, 1);
            return { true, lua_rawlen(L, index) };
        }

        if (luaL_getmetafield(L, index, "__object") != LUA_TNIL) {
            lua_pop(L, 1);
            return { false, 0 };
        }
    }

    // 先获取表长度，避免多次调用
    auto len = static_cast<lua_Integer>(lua_rawlen(L, index));
    
    // test first key
    lua_pushnil(L);
    if (lua_next(L, index) == 0) // empty table
        return { false, 0 };

    lua_Integer firstkey = lua_isinteger(L, -2) ? lua_tointeger(L, -2) : 0;
    lua_pop(L, 2);

    if (firstkey <= 0) {
        return { false, 0 };
    } else if (firstkey == 1) {
        /*
        * https://www.lua.org/manual/5.4/manual.html#3.4.7
        * The length operator applied on a table returns a border in that table.
        * A border in a table t is any natural number that satisfies the following condition :
        * (border == 0 or t[border] ~= nil) and t[border + 1] == nil
        */
        lua_pushinteger(L, len);
        if (lua_next(L, index)) // has more fields?
        {
            lua_pop(L, 2);
            return { false, 0 };
        }
    }

    if (firstkey > len)
        return { false, 0 };

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
        return { false, 0 };
    }
    return { true, len };
}

template<bool format>
static void
encode_table_object(lua_State* L, buffer* writer, int idx, int depth, const json_config* cfg) {
    size_t i = 0;
    writer->write_back('{');
    lua_pushnil(L); // [table, nil]
    while (lua_next(L, idx)) {
        if (i++ > 0)
            writer->write_back(',');
        format_new_line<format>(writer);
        switch (auto key_type = lua_type(L, -2)) {
            case LUA_TSTRING: {
                format_space<format>(writer, depth);
                auto key = moon::lua_check<std::string_view>(L, -2);
                writer->prepare(key.size() + 4);
                writer->unsafe_write_back('\"');
                writer->unsafe_write_back(key);
                writer->unsafe_write_back('\"');
                writer->unsafe_write_back(':');
                if constexpr (format)
                    writer->unsafe_write_back(' ');
                encode_one<format>(L, writer, -1, depth, cfg);
                break;
            }
            case LUA_TNUMBER: {
                if (lua_isinteger(L, -2) && cfg->enable_number_key) {
                    format_space<format>(writer, depth);
                    lua_Integer key = lua_tointeger(L, -2);
                    writer->prepare(50);
                    writer->unsafe_write_back('\"');
                    writer->write_chars(key);
                    writer->unsafe_write_back('\"');
                    writer->unsafe_write_back(':');
                    if constexpr (format)
                        writer->unsafe_write_back(' ');
                    encode_one<format>(L, writer, -1, depth, cfg);
                } else {
                    throw lua_json_error::format("json encode: unsupported number key type");
                }
                break;
            }
            default:
                throw lua_json_error::format(
                    "json encode: unsupported key type: %s",
                    lua_typename(L, key_type)
                );
        }
        lua_pop(L, 1);
    }

    if (i == 0 && cfg->empty_as_array) {
        writer->revert(1);
        writer->write_back('[');
        writer->write_back(']');
    } else {
        if (i > 0) {
            format_new_line<format>(writer);
            format_space<format>(writer, depth - 1);
        }
        writer->write_back('}');
    }
}

template<bool format>
static void encode_table_array(
    lua_State* L,
    size_t size,
    buffer* writer,
    int idx,
    int depth,
    const json_config* cfg
) {
    size_t bsize = writer->size();
    writer->write_back('[');
    for (size_t i = 1; i <= size; i++) {
        if (i == 1)
            format_new_line<format>(writer);
        format_space<format>(writer, depth);
        lua_rawgeti(L, idx, i);
        if (lua_isnil(L, -1) && !cfg->enable_sparse_array) {
            lua_pop(L, 1);
            writer->revert(writer->size() - bsize);
            return encode_table_object<format>(L, writer, idx, depth, cfg);
        }
        encode_one<format>(L, writer, -1, depth, cfg);
        lua_pop(L, 1);
        if (i < size)
            writer->write_back(',');
        format_new_line<format>(writer);
    }
    format_space<format>(writer, depth - 1);
    writer->write_back(']');
}

template<bool format>
static void encode_table(lua_State* L, buffer* writer, int idx, int depth, const json_config* cfg) {
    if ((++depth) > MAX_DEPTH)
        throw lua_json_error::format(
            "nested too deep (depth=%d, max=%d)",
            depth,
            MAX_DEPTH
        );

    if (idx < 0) {
        idx = lua_gettop(L) + idx + 1;
    }

    luaL_checkstack(L, 6, "json.encode_table");

    if (auto [b, n] = is_array(L, idx, cfg); b) {
        encode_table_array<format>(L, n, writer, idx, depth, cfg);
    } else {
        encode_table_object<format>(L, writer, idx, depth, cfg);
    }
}

static int encode(lua_State* L) {
    json_config* cfg = json_fetch_config(L);
    luaL_checkany(L, 1);

    try {
        buffer* writer = get_thread_encode_buffer();
        encode_one<false>(L, writer, 1, 0, cfg);
        lua_pushlstring(L, writer->data(), writer->size());
        return 1;
    } catch (const lua_json_error& ex) {
        lua_pushstring(L, ex.what());
    }
    return lua_error(L);
}

static int pretty_encode(lua_State* L) {
    json_config* cfg = json_fetch_config(L);
    luaL_checkany(L, 1);
    try {
        buffer* writer = get_thread_encode_buffer();
        lua_settop(L, 1);
        encode_one<true>(L, writer, 1, 0, cfg);
        lua_pushlstring(L, writer->data(), writer->size());
        return 1;
    } catch (const lua_json_error& ex) {
        lua_pushstring(L, ex.what());
    }
    return lua_error(L);
}

static void decode_one(lua_State* L, yyjson_val* value, json_config* cfg) {
    yyjson_type type = yyjson_get_type(value);
    switch (type) {
        case YYJSON_TYPE_ARR: {
            luaL_checkstack(L, 6, "json.decode.array");
            lua_createtable(L, (int)yyjson_arr_size(value), 0);
            if (cfg->has_metatfield) {
                if (luaL_newmetatable(L, JSON_ARRAY_MT.data())) {
                    luaL_rawsetfield(L, -3, "__array", lua_pushboolean(L, 1));
                }
                lua_setmetatable(L, -2);
            }
            lua_Integer pos = 1;
            yyjson_arr_iter iter;
            yyjson_arr_iter_init(value, &iter);
            while (nullptr != (value = yyjson_arr_iter_next(&iter))) {
                decode_one(L, value, cfg);
                lua_rawseti(L, -2, pos++);
            }
            break;
        }
        case YYJSON_TYPE_OBJ: {
            luaL_checkstack(L, 6, "json.decode.object");
            lua_createtable(L, 0, (int)yyjson_obj_size(value));
            if (cfg->has_metatfield) {
                if (luaL_newmetatable(L, JSON_OBJECT_MT.data())) {
                    luaL_rawsetfield(L, -3, "__object", lua_pushboolean(L, 1));
                }
                lua_setmetatable(L, -2);
            }

            yyjson_val *key, *val;
            yyjson_obj_iter iter;
            yyjson_obj_iter_init(value, &iter);
            while (nullptr != (key = yyjson_obj_iter_next(&iter))) {
                val = yyjson_obj_iter_get_val(key);
                std::string_view view { unsafe_yyjson_get_str(key), unsafe_yyjson_get_len(key) };
                if (view.size() > 0) {
                    if (char c = view.data()[0];
                        (c == '-' || (c >= '0' && c <= '9')) && cfg->enable_number_key)
                    {
                        const char* last = view.data() + view.size();
                        int64_t v = 0;
                        auto [p, ec] = std::from_chars(view.data(), last, v);
                        if (ec == std::errc() && p == last)
                            lua_pushinteger(L, v);
                        else
                            lua_pushlstring(L, view.data(), view.size());
                    } else {
                        lua_pushlstring(L, view.data(), view.size());
                    }
                    decode_one(L, val, cfg);
                    lua_rawset(L, -3);
                }
            }
            break;
        }
        case YYJSON_TYPE_NUM: {
            switch (yyjson_get_subtype(value)) {
                case YYJSON_SUBTYPE_UINT: {
                    lua_pushinteger(L, (int64_t)unsafe_yyjson_get_uint(value));
                    break;
                }
                case YYJSON_SUBTYPE_SINT: {
                    lua_pushinteger(L, unsafe_yyjson_get_sint(value));
                    break;
                }
                case YYJSON_SUBTYPE_REAL: {
                    lua_pushnumber(L, unsafe_yyjson_get_real(value));
                    break;
                }
                default:
                    break;
            }
            break;
        }
        case YYJSON_TYPE_STR: {
            lua_pushlstring(L, unsafe_yyjson_get_str(value), unsafe_yyjson_get_len(value));
            break;
        }
        case YYJSON_TYPE_BOOL:
            lua_pushboolean(L, (yyjson_get_subtype(value) == YYJSON_SUBTYPE_TRUE) ? 1 : 0);
            break;
        case YYJSON_TYPE_NULL: {
            lua_pushlightuserdata(L, nullptr);
            break;
        }
        default:
            break;
    }
}

static int decode(lua_State* L) {
    size_t len = 0;
    const char* str = nullptr;
    if (lua_type(L, 1) == LUA_TSTRING) {
        str = luaL_checklstring(L, 1, &len);
    } else {
        str = reinterpret_cast<const char*>(lua_touserdata(L, 1));
        len = luaL_checkinteger(L, 2);
    }

    if (nullptr == str || str[0] == '\0')
        return 0;

    json_config* cfg = json_fetch_config(L);

    lua_settop(L, 1);

    yyjson_read_err err;
    yyjson_doc* doc = yyjson_read_opts((char*)str, len, 0, &allocator, &err);
    if (nullptr == doc) {
        return luaL_error(
            L,
            "decode error: %s code: %d at position: %d\n",
            err.msg,
            (int)err.code,
            (int)err.pos
        );
    }
    decode_one(L, yyjson_doc_get_root(doc), cfg);
    yyjson_doc_free(doc);
    return 1;
}

static int concat(lua_State* L) {
    if (lua_type(L, 1) == LUA_TSTRING) {
        size_t size;
        const char* sz = lua_tolstring(L, -1, &size);
        auto buf = std::make_unique<moon::buffer>(BUFFER_OPTION_CHEAP_PREPEND + size);
        buf->commit_unchecked(BUFFER_OPTION_CHEAP_PREPEND);
        buf->write_back({ sz, size });
        buf->consume_unchecked(BUFFER_OPTION_CHEAP_PREPEND);
        lua_pushlightuserdata(L, buf.release());
        return 1;
    }

    luaL_checktype(L, 1, LUA_TTABLE);

    lua_settop(L, 1);

    json_config* cfg = json_fetch_config(L);

    auto buf = std::make_unique<moon::buffer>(cfg->concat_buffer_size);
    buf->commit_unchecked(BUFFER_OPTION_CHEAP_PREPEND);
    try {
        auto array_size = (int)lua_rawlen(L, 1);
        for (int i = 1; i <= array_size; i++) {
            lua_rawgeti(L, 1, i);
            switch (int t = lua_type(L, -1)) {
                case LUA_TNUMBER: {
                    if (lua_isinteger(L, -1))
                        buf->write_chars(lua_tointeger(L, -1));
                    else
                        write_number(buf.get(), lua_tonumber(L, -1));
                    break;
                }
                case LUA_TBOOLEAN: {
                    buf->write_back(bool_string[lua_toboolean(L, -1)]);
                    break;
                }
                case LUA_TSTRING: {
                    size_t size;
                    const char* sz = lua_tolstring(L, -1, &size);
                    buf->write_back({ sz, size });
                    break;
                }
                case LUA_TTABLE: {
                    encode_one<false>(L, buf.get(), -1, 0, cfg);
                    break;
                }
                default:
                    throw lua_json_error::format(
                        "json concat: unsupported value type: %s at index %d",
                        lua_typename(L, t),
                        i
                    );
            }
            lua_pop(L, 1);
        }
        buf->consume_unchecked(BUFFER_OPTION_CHEAP_PREPEND);
        lua_pushlightuserdata(L, buf.release());
        return 1;
    } catch (const lua_json_error& ex) {
        lua_pushstring(L, ex.what());
    }
    return lua_error(L);
}

static void write_resp(buffer* buf, const char* cmd, size_t size) {
    buf->write_back({ "\r\n$", 3 });
    buf->write_chars(size);
    buf->write_back({ "\r\n", 2 });
    buf->write_back({ cmd, size });
}

static void concat_resp_one(buffer* buf, lua_State* L, int i, json_config* cfg) {
    int t = lua_type(L, i);
    switch (t) {
        case LUA_TNIL: {
            std::string_view sv = "\r\n$-1"sv;
            buf->write_back(sv);
            break;
        }
        case LUA_TNUMBER: {
            if (lua_isinteger(L, i)) {
                std::string s = std::to_string(lua_tointeger(L, i));
                write_resp(buf, s.data(), s.size());
            } else {
                std::string s = std::to_string(lua_tonumber(L, i));
                write_resp(buf, s.data(), s.size());
            }
            break;
        }
        case LUA_TBOOLEAN: {
            auto str = bool_string[lua_toboolean(L, i)];
            write_resp(buf, str.data(), str.size());
            break;
        }
        case LUA_TSTRING: {
            size_t msize;
            const char* sz = lua_tolstring(L, i, &msize);
            write_resp(buf, sz, msize);
            break;
        }
        case LUA_TTABLE: {
            if (luaL_getmetafield(L, i, "__redis") != LUA_TNIL) {
                lua_pop(L, 1);
                auto size = lua_rawlen(L, i);
                for (unsigned n = 1; n <= size; n++) {
                    lua_rawgeti(L, i, n);
                    concat_resp_one(buf, L, -1, cfg);
                    lua_pop(L, 1);
                }
            } else {
                buffer* writer = get_thread_encode_buffer();
                encode_one<false>(L, writer, i, 0, cfg);
                write_resp(buf, writer->data(), writer->size());
            }
            break;
        }
        default:
            throw lua_json_error::format(
                "concat_resp_one: unsupported value type: %s",
                lua_typename(L, t)
            );
    }
}

static int concat_resp(lua_State* L) {
    int n = lua_gettop(L);
    if (0 == n)
        return 0;

    json_config* cfg = json_fetch_config(L);

    auto buf = std::make_unique<moon::buffer>(cfg->concat_buffer_size);
    try {
        int64_t hash = 1;
        if (lua_type(L, 2) == LUA_TTABLE) {
            size_t len = 0;
            const char* key = lua_tolstring(L, 1, &len);
            if (len > 0) {
                std::string_view hash_part;
                if (n > 1) {
                    const char* field = lua_tolstring(L, 2, &len);
                    if (len > 0)
                        hash_part = std::string_view { field, len };
                }

                if (n > 2 && (key[0] == 'h' || key[0] == 'H')) {
                    const char* field = lua_tolstring(L, 3, &len);
                    if (len > 0)
                        hash_part = std::string_view { field, len };
                }

                if (!hash_part.empty())
                    hash =
                        static_cast<uint32_t>(moon::hash_range(hash_part.begin(), hash_part.end()));
            }
        }

        buf->write_back('*');
        buf->write_chars(n);

        for (int i = 1; i <= n; i++) {
            concat_resp_one(buf.get(), L, i, cfg);
        }

        buf->write_back({ "\r\n", 2 });
        lua_pushlightuserdata(L, buf.release());
        lua_pushinteger(L, hash);
        return 2;
    } catch (const lua_json_error& ex) {
        lua_pushstring(L, ex.what());
    }
    return lua_error(L);
}

static int json_object(lua_State* L) {
    if (lua_isinteger(L, 1)) {
        auto n = (int)luaL_optinteger(L, 1, 16);
        lua_createtable(L, 0, n);
    }
    luaL_checktype(L, -1, LUA_TTABLE);
    if (luaL_newmetatable(L, JSON_OBJECT_MT.data())) {
        luaL_rawsetfield(L, -3, "__object", lua_pushboolean(L, 1));
    }
    lua_setmetatable(L, -2); // set userdata metatable
    return 1;
}

static int json_array(lua_State* L) {
    if (lua_isinteger(L, 1)) {
        auto n = (int)luaL_optinteger(L, 1, 16);
        lua_createtable(L, n, 1);
    }
    luaL_checktype(L, -1, LUA_TTABLE);
    if (luaL_newmetatable(L, JSON_ARRAY_MT.data())) {
        luaL_rawsetfield(L, -3, "__array", lua_pushboolean(L, 1));
    }
    lua_setmetatable(L, -2); // set userdata metatable
    return 1;
}

extern "C" {
int LUAMOD_API luaopen_json(lua_State* L) {
    luaL_Reg l[] = {
        { "encode", encode },
        { "pretty_encode", pretty_encode },
        { "decode", decode },
        { "concat", concat },
        { "concat_resp", concat_resp },
        { "options", json_options },
        { "object", json_object },
        { "array", json_array },
        { "null", nullptr },
        { nullptr, nullptr },
    };

    luaL_checkversion(L);
    luaL_newlibtable(L, l);
    json_create_config(L);
    luaL_setfuncs(L, l, 1);

    lua_pushlightuserdata(L, nullptr);
    lua_setfield(L, -2, "null");
    return 1;
}
}
