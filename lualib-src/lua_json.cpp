#include "lua.hpp"
#include "yyjson/yyjson.h"
#include <string_view>
#include <charconv>
#include <codecvt>
#include "common/buffer.hpp"
#include "common/string.hpp"

#ifdef MOON_ENABLE_MIMALLOC
static void* my_malloc_func(void*, size_t size) {
    return mi_malloc(size);
}

static void* my_realloc_func(void*, void* ptr, size_t size) {
    return mi_realloc(ptr, size);
}

static void my_free_func(void*, void* ptr) {
    mi_free(ptr);
}

static const yyjson_alc allocator = {
    my_malloc_func,
    my_realloc_func,
    my_free_func,
    nullptr
};
#else
static const yyjson_alc allocator = { nullptr };
#endif

using namespace std::literals::string_view_literals;
using namespace moon;

static constexpr std::string_view json_null = "null"sv;
static constexpr std::string_view json_true = "true"sv;
static constexpr std::string_view json_false = "false"sv;

static constexpr int EMPTY_AS_ARRAY = 1;

static constexpr int MAX_DEPTH = 64;

static const char char2escape[256] = {
    'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'b', 't', 'n', 'u', 'f', 'r', 'u', 'u', 'u', 'u', 'u', 'u', // 0~19
    'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 0, 0, '"', 0, 0, 0, 0, 0,               // 20~39
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                                         // 40~59
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                                         // 60~79
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '\\', 0, 0, 0, 0, 0, 0, 0,                                      // 80~99
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                                         // 100~119
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                                         // 120~139
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                                         // 140~159
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                                         // 160~179
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                                         // 180~199
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                                         // 200~219
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                                         // 220~239
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                                                     // 240~256
};

static const char hex_digits[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

template <bool format>
static void format_new_line(buffer* writer)
{
    if constexpr (format)
    {
        writer->write_back('\n');
    }
}

template <bool format>
static void format_space(buffer* writer, int n)
{
    if constexpr (format)
    {
        writer->prepare(2 * size_t(n));
        for (int i = 0; i < n; ++i)
        {
            writer->unsafe_write_back(' ');
            writer->unsafe_write_back(' ');
        }
    }
}

template <bool format>
static void encode_table(lua_State* L, buffer* writer, int idx, int depth);

template <bool format>
static void encode_one(lua_State* L, buffer* writer, int idx, int depth = 0)
{
    int t = lua_type(L, idx);
    switch (t)
    {
    case LUA_TBOOLEAN:
    {
        if (lua_toboolean(L, idx))
            writer->write_back(json_true.data(), json_true.size());
        else
            writer->write_back(json_false.data(), json_false.size());
        return;
    }
    case LUA_TNUMBER:
    {
        if (lua_isinteger(L, idx))
            writer->write_chars(lua_tointeger(L, idx));
        else
            writer->write_chars(lua_tonumber(L, idx));
        return;
    }
    case LUA_TSTRING:
    {
        size_t len = 0;
        const char* str = lua_tolstring(L, idx, &len);
        writer->prepare(len * 6 + 2);
        writer->unsafe_write_back('\"');
        for (size_t i = 0; i < len; ++i)
        {
            unsigned char ch = (unsigned char)str[i];
            char esc = char2escape[ch];
            if (!esc)
            {
                writer->unsafe_write_back((char)ch);
            }
            else
            {
                writer->unsafe_write_back('\\');
                writer->unsafe_write_back(esc);
                if (esc == 'u')
                {
                    writer->unsafe_write_back('0');
                    writer->unsafe_write_back('0');
                    writer->unsafe_write_back(hex_digits[(unsigned char)esc >> 4]);
                    writer->unsafe_write_back(hex_digits[(unsigned char)esc & 0xF]);
                }
            }
        }
        writer->unsafe_write_back('\"');
        return;
    }
    case LUA_TTABLE:
    {
        encode_table<format>(L, writer, idx, depth);
        return;
    }
    case LUA_TNIL:
    {
        writer->write_back(json_null.data(), json_null.size());
        return;
    }
    case LUA_TLIGHTUSERDATA:
    {
        if (lua_touserdata(L, idx) == nullptr)
        {
            writer->write_back(json_null.data(), json_null.size());
            return;
        }
        break;
    }
    }
    throw std::logic_error{ moon::format("json encode: unsupport value type : %s", lua_typename(L, t)) };
}

static inline size_t array_size(lua_State* L, int index)
{
    // test first key
    lua_pushnil(L);
    if (lua_next(L, index) == 0) // empty table
        return 0;

    lua_Integer firstkey = lua_isinteger(L, -2) ? lua_tointeger(L, -2) : 0;
    lua_pop(L, 2);

    if (firstkey <= 0)
    {
        return 0;
    }
    else if (firstkey == 1)
    {
        /*
        * https://www.lua.org/manual/5.4/manual.html#6.6
        * The length operator applied on a table returns a border in that table.
        * A border in a table t is any natural number that satisfies the following condition :
        * (border == 0 or t[border] ~= nil) and t[border + 1] == nil
        */
        auto len = (lua_Integer)lua_rawlen(L, index);
        lua_pushinteger(L, len);
        if (lua_next(L, index)) // has more fields?
        {
            lua_pop(L, 2);
            return 0;
        }
        return len;
    }

    auto len = (lua_Integer)lua_rawlen(L, index);
    if (firstkey > len)
        return 0;

    lua_pushnil(L);
    while (lua_next(L, index) != 0)
    {
        if (lua_isinteger(L, -2))
        {
            lua_Integer x = lua_tointeger(L, -2);
            if (x > 0 && x <= len)
            {
                lua_pop(L, 1);
                continue;
            }
        }
        lua_pop(L, 2);
        return 0;
    }
    return len;
}

template <bool format>
static void encode_table(lua_State* L, buffer* writer, int idx, int depth)
{
    if ((++depth) > MAX_DEPTH)
        throw std::logic_error{ "nested too depth" };

    if (idx < 0)
    {
        idx = lua_gettop(L) + idx + 1;
    }

    luaL_checkstack(L, 6, "json.encode_table");

    if (size_t size = array_size(L, idx); size > 0)
    {
        writer->write_back('[');
        for (size_t i = 1; i <= size; i++)
        {
            if (i == 1)
                format_new_line<format>(writer);
            format_space<format>(writer, depth);
            lua_rawgeti(L, idx, i);
            encode_one<format>(L, writer, -1, depth);
            lua_pop(L, 1);
            if (i < size)
                writer->write_back(',');
            format_new_line<format>(writer);
        }
        format_space<format>(writer, depth - 1);
        writer->write_back(']');
    }
    else
    {
        size_t i = 0;
        writer->write_back('{');
        lua_pushnil(L); // [table, nil]
        while (lua_next(L, idx))
        {
            if (i++ > 0)
                writer->write_back(',');
            format_new_line<format>(writer);
            int key_type = lua_type(L, -2);
            switch (key_type)
            {
            case LUA_TSTRING:
            {
                format_space<format>(writer, depth);
                size_t len = 0;
                const char* key = lua_tolstring(L, -2, &len);
                writer->write_back('\"');
                writer->write_back(key, len);
                writer->write_back('\"');
                writer->write_back(':');
                if constexpr (format)
                    writer->write_back(' ');
                encode_one<format>(L, writer, -1, depth);
                break;
            }
            case LUA_TNUMBER:
            {
                if (lua_isinteger(L, -2))
                {
                    format_space<format>(writer, depth);
                    lua_Integer key = lua_tointeger(L, -2);
                    writer->write_back('\"');
                    writer->write_chars(key);
                    writer->write_back('\"');
                    writer->write_back(':');
                    if constexpr (format)
                        writer->write_back(' ');
                    encode_one<format>(L, writer, -1, depth);
                }
                else
                {
                    throw std::logic_error{ "json encode: not support double type 'key'." };
                }
                break;
            }
            default:
                throw std::logic_error{ moon::format("json encode: unsupport key type : %s", lua_typename(L, key_type)) };
            }
            lua_pop(L, 1);
        }

        if (i == 0 && writer->has_flag(EMPTY_AS_ARRAY))
        {
            writer->revert(1);
            writer->write_back('[');
            writer->write_back(']');
        }
        else
        {
            if (i > 0)
            {
                format_new_line<format>(writer);
                format_space<format>(writer, depth - 1);
            }
            writer->write_back('}');
        }
    }
}

static int encode(lua_State* L)
{
    try
    {
        luaL_checkany(L, 1);
        buffer writer(512);
        if (lua_toboolean(L, 2))
            writer.set_flag(EMPTY_AS_ARRAY);
        bool format = lua_toboolean(L, 3);
        lua_settop(L, 1);
        if (!format)
            encode_one<false>(L, &writer, 1);
        else
            encode_one<true>(L, &writer, 1);
        lua_pushlstring(L, writer.data(), writer.size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        return luaL_error(L, ex.what());
    }
}

static void decode_one(lua_State* L, yyjson_val* value, bool null_as_userdata)
{
    yyjson_type type = yyjson_get_type(value);
    switch (type)
    {
    case YYJSON_TYPE_ARR:
    {
        luaL_checkstack(L, 6, "json.decode.array");
        lua_createtable(L, (int)yyjson_arr_size(value), 0);
        lua_Integer pos = 1;
        yyjson_arr_iter iter;
        yyjson_arr_iter_init(value, &iter);
        while (nullptr != (value = yyjson_arr_iter_next(&iter)))
        {
            decode_one(L, value, null_as_userdata);
            lua_rawseti(L, -2, pos++);
        }
        break;
    }
    case YYJSON_TYPE_OBJ:
    {
        luaL_checkstack(L, 6, "json.decode.object");
        lua_createtable(L, 0, (int)yyjson_obj_size(value));

        yyjson_val* key, * val;
        yyjson_obj_iter iter;
        yyjson_obj_iter_init(value, &iter);
        while (nullptr != (key = yyjson_obj_iter_next(&iter)))
        {
            val = yyjson_obj_iter_get_val(key);
            std::string_view view{ unsafe_yyjson_get_str(key), unsafe_yyjson_get_len(key) };
            if (view.size() > 0)
            {
                char c = view.data()[0];
                if (c == '-' || (c >= '0' && c <= '9'))
                {
                    const char* last = view.data() + view.size();
                    int64_t v = 0;
                    auto [p, ec] = std::from_chars(view.data(), last, v);
                    if (ec == std::errc() && p == last)
                        lua_pushinteger(L, v);
                    else
                        lua_pushlstring(L, view.data(), view.size());
                }
                else
                {
                    lua_pushlstring(L, view.data(), view.size());
                }
                decode_one(L, val, null_as_userdata);
                lua_rawset(L, -3);
            }
        }
        break;
    }
    case YYJSON_TYPE_NUM:
    {
        yyjson_subtype subtype = yyjson_get_subtype(value);
        switch (subtype)
        {
        case YYJSON_SUBTYPE_UINT:
        {
            lua_pushinteger(L, (int64_t)unsafe_yyjson_get_uint(value));
            break;
        }
        case YYJSON_SUBTYPE_SINT:
        {
            lua_pushinteger(L, unsafe_yyjson_get_sint(value));
            break;
        }
        case YYJSON_SUBTYPE_REAL:
        {
            lua_pushnumber(L, unsafe_yyjson_get_real(value));
            break;
        }
        }
        break;
    }
    case YYJSON_TYPE_STR:
    {
        lua_pushlstring(L, unsafe_yyjson_get_str(value), unsafe_yyjson_get_len(value));
        break;
    }
    case YYJSON_TYPE_BOOL:
        lua_pushboolean(L, (yyjson_get_subtype(value) == YYJSON_SUBTYPE_TRUE) ? 1 : 0);
        break;
    case YYJSON_TYPE_NULL:
    {
        if (null_as_userdata)
            lua_pushlightuserdata(L, nullptr);
        else
            lua_pushnil(L);
        break;
    }
    default:
        break;
    }
}

static int decode(lua_State* L)
{
    size_t len = 0;
    const char* str = nullptr;
    bool null_as_userdata = false;
    if (lua_type(L, 1) == LUA_TSTRING)
    {
        str = luaL_checklstring(L, 1, &len);
        null_as_userdata = lua_toboolean(L, 2);
    }
    else
    {
        str = reinterpret_cast<const char*>(lua_touserdata(L, 1));
        len = luaL_checkinteger(L, 2);
        null_as_userdata = lua_toboolean(L, 3);
    }

    if (nullptr == str || str[0] == '\0')
        return 0;

    lua_settop(L, 1);

    yyjson_read_err err;
    yyjson_doc* doc = yyjson_read_opts((char*)str, len, 0, (allocator.malloc) ? &allocator : nullptr, &err);
    if (nullptr == doc)
    {
        return luaL_error(L, "decode error: %s code: %d at position: %d\n", err.msg, (int)err.code, (int)err.pos);
    }
    decode_one(L, yyjson_doc_get_root(doc), null_as_userdata);
    yyjson_doc_free(doc);
    return 1;
}

static int concat(lua_State* L)
{
    if (lua_type(L, 1) == LUA_TSTRING)
    {
        size_t size;
        const char* sz = lua_tolstring(L, -1, &size);
        auto buf = new moon::buffer(size, 16);
        buf->write_back(sz, size);
        lua_pushlightuserdata(L, buf);
        return 1;
    }

    luaL_checktype(L, 1, LUA_TTABLE);

    lua_settop(L, 1);

    auto buf = new moon::buffer(512, 16);
    try
    {
        int array_size = (int)lua_rawlen(L, 1);
        for (int i = 1; i <= array_size; i++)
        {
            lua_rawgeti(L, 1, i);
            int t = lua_type(L, -1);
            switch (t)
            {
            case LUA_TNUMBER:
            {
                if (lua_isinteger(L, -1))
                    buf->write_chars(lua_tointeger(L, -1));
                else
                    buf->write_chars(lua_tonumber(L, -1));
                break;
            }
            case LUA_TBOOLEAN:
            {
                if (lua_toboolean(L, -1))
                    buf->write_back(json_true.data(), json_true.size());
                else
                    buf->write_back(json_false.data(), json_false.size());
                break;
            }
            case LUA_TSTRING:
            {
                size_t size;
                const char* sz = lua_tolstring(L, -1, &size);
                buf->write_back(sz, size);
                break;
            }
            case LUA_TTABLE:
            {
                encode_one<false>(L, buf, -1);
                break;
            }
            default:
                throw std::logic_error{ moon::format("json encode: unsupport value type : %s", lua_typename(L, t)) };
                break;
            }
            lua_pop(L, 1);
        }
        lua_pushlightuserdata(L, buf);
    }
    catch (const std::exception& ex)
    {
        delete buf;
        return luaL_error(L, ex.what());
    }
    return 1;
}

static void write_resp(moon::buffer* buf, const char* cmd, size_t size)
{
    buf->write_back("\r\n$", 3);
    buf->write_chars(size);
    buf->write_back("\r\n", 2);
    buf->write_back(cmd, size);
}

static void concat_resp_one(moon::buffer* buf, lua_State* L, int i)
{
    int t = lua_type(L, i);
    switch (t)
    {
    case LUA_TNIL:
    {
        std::string_view sv = "\r\n$-1"sv;
        buf->write_back(sv.data(), sv.size());
        break;
    }
    case LUA_TNUMBER:
    {
        if (lua_isinteger(L, i))
        {
            std::string s = std::to_string(lua_tointeger(L, i));
            write_resp(buf, s.data(), s.size());
        }
        else
        {
            std::string s = std::to_string(lua_tonumber(L, i));
            write_resp(buf, s.data(), s.size());
        }
        break;
    }
    case LUA_TBOOLEAN:
    {
        if (lua_toboolean(L, i))
            write_resp(buf, json_true.data(), json_true.size());
        else
            write_resp(buf, json_false.data(), json_false.size());
        break;
    }
    case LUA_TSTRING:
    {
        size_t msize;
        const char* sz = lua_tolstring(L, i, &msize);
        write_resp(buf, sz, msize);
        break;
    }
    case LUA_TTABLE:
    {
        if (luaL_getmetafield(L, i, "__redis") != LUA_TNIL)
        {
            lua_pop(L, 1);
            auto size = lua_rawlen(L, i);
            for (unsigned n = 1; n <= size; n++)
            {
                lua_rawgeti(L, i, n);
                concat_resp_one(buf, L, -1);
                lua_pop(L, 1);
            }
        }
        else
        {
            buffer writer(512);
            encode_one<false>(L, &writer, i, 0);
            write_resp(buf, writer.data(), writer.size());
        }
        break;
    }
    default:
        throw std::logic_error{ moon::format("json encode: unsupport value type : %s", lua_typename(L, t)) };
        break;
    }
}

static int concat_resp(lua_State* L)
{
    int n = lua_gettop(L);
    if (0 == n)
        return 0;
    auto buf = new moon::buffer(512, 16);
    try
    {
        buf->write_back('*');
        buf->write_chars(n);

        for (int i = 1; i <= n; i++)
        {
            concat_resp_one(buf, L, i);
        }

        buf->write_back("\r\n", 2);
        lua_pushlightuserdata(L, buf);
    }
    catch (const std::exception& ex)
    {
        delete buf;
        return luaL_error(L, ex.what());
    }
    return 1;
}

extern "C"
{
    int LUAMOD_API luaopen_json(lua_State* L)
    {
        luaL_Reg l[] = {
            {"encode", encode},
            {"decode", decode},
            {"concat", concat},
            {"concat_resp", concat_resp},
            {nullptr, nullptr} };

        luaL_newlib(L, l);
        lua_pushstring(L, "null");
        lua_pushlightuserdata(L, nullptr);
        lua_rawset(L, -3);
        return 1;
    }
}
