#include "lua.hpp"
#include "yyjson/yyjson.h"
#include <string_view>
#include <charconv>
#include <codecvt>
#include "common/buffer.hpp"
#include "common/hash.hpp"
#include "common/string.hpp"

#ifdef MOON_ENABLE_MIMALLOC
static void* json_malloc(void*, size_t size) {
    return mi_malloc(size);
}

static void* json_realloc(void*, void* ptr,  size_t , size_t size)
{
    return mi_realloc(ptr, size);
}

static void json_free(void*, void* ptr)
{
    mi_free(ptr);
}

static const yyjson_alc allocator = {json_malloc, json_realloc, json_free,nullptr};
#else
static const yyjson_alc allocator = { nullptr };
#endif

using namespace std::literals::string_view_literals;
using namespace moon;

static constexpr std::string_view json_null = "null"sv;
static constexpr std::string_view json_true = "true"sv;
static constexpr std::string_view json_false = "false"sv;

static constexpr int MAX_DEPTH = 64;

static constexpr size_t DEFAULT_CONCAT_BUFFER_SIZE = 512;

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

static buffer* get_thread_encode_buffer()
{
    static thread_local buffer thread_encode_buffer{DEFAULT_CONCAT_BUFFER_SIZE};
    thread_encode_buffer.clear();
    return &thread_encode_buffer;
}

struct json_config {
    bool empty_as_array = true;
    bool enable_number_key = true;
    bool enable_sparse_array = false;
    size_t concat_buffer_size = DEFAULT_CONCAT_BUFFER_SIZE;
    uint16_t concat_buffer_head_size = buffer::DEFAULT_HEAD_RESERVE;
};

static int json_destroy_config(lua_State *L)
{
    json_config *cfg = (json_config *)lua_touserdata(L, 1);
    if (cfg)
        std::destroy_at(cfg);
    return 0;
}

static void json_create_config(lua_State *L){
    void* mem = lua_newuserdatauv(L, sizeof(json_config), 0);
    new (mem) json_config{};
    lua_newtable(L);
    lua_pushcfunction(L, json_destroy_config);
    lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -2);
}

static json_config *json_fetch_config(lua_State *L)
{
    json_config *cfg = (json_config*)lua_touserdata(L, lua_upvalueindex(1));
    if (!cfg)
        luaL_error(L, "Unable to fetch json configuration");
    return cfg;
}

static int json_options(lua_State* L)
{
    json_config* cfg = json_fetch_config(L);
    size_t len = 0;
    const char* name = luaL_checklstring(L, 1, &len);
    int top = lua_gettop(L);
    switch (moon::chash_string(name, len))
    {
    case "encode_empty_as_array"_csh:
    {
        bool v = cfg->empty_as_array;
        cfg->empty_as_array = static_cast<bool>(lua_toboolean(L, 2));
        lua_pushboolean(L, v ? 1 : 0);
        break;
    }
    case "enable_number_key"_csh:
    {
        bool v = cfg->enable_number_key;
        cfg->enable_number_key = static_cast<bool>(lua_toboolean(L, 2));
        lua_pushboolean(L, v ? 1 : 0);
        break;
    }
    case "enable_sparse_array"_csh:
    {
        bool v = cfg->enable_sparse_array;
        cfg->enable_sparse_array = static_cast<bool>(lua_toboolean(L, 2));
        lua_pushboolean(L, v ? 1 : 0);
        break;
    }
    case "concat_buffer_size"_csh:
    {
        auto concat_buffer_size = cfg->concat_buffer_size;
        auto concat_buffer_head_size = cfg->concat_buffer_head_size;
        cfg->concat_buffer_size = static_cast<uint32_t>(luaL_checkinteger(L, 2));
        cfg->concat_buffer_head_size = static_cast<uint16_t>(luaL_checkinteger(L, 3));
        lua_pushinteger(L, static_cast<lua_Integer>(concat_buffer_size));
        lua_pushinteger(L, static_cast<lua_Integer>(concat_buffer_head_size));
        break;
    }
    default:
        return luaL_error(L, "Invalid json.options '%s'", name);
    }
    return lua_gettop(L) - top;
}

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
static void encode_one(lua_State* L, buffer* writer, int idx, int depth, json_config* cfg)
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
        encode_table<format>(L, writer, idx, depth, cfg);
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
    throw std::logic_error{ std::string("json encode: unsupport value type :")+lua_typename(L, t)};
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
        * https://www.lua.org/manual/5.4/manual.html#3.4.7
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
static void encode_table_object(lua_State* L, buffer* writer, int idx, int depth, json_config* cfg)
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
            encode_one<format>(L, writer, -1, depth, cfg);
            break;
        }
        case LUA_TNUMBER:
        {
            if (lua_isinteger(L, -2) && cfg->enable_number_key)
            {
                format_space<format>(writer, depth);
                lua_Integer key = lua_tointeger(L, -2);
                writer->write_back('\"');
                writer->write_chars(key);
                writer->write_back('\"');
                writer->write_back(':');
                if constexpr (format)
                    writer->write_back(' ');
                encode_one<format>(L, writer, -1, depth, cfg);
            }
            else
            {
                throw std::logic_error{ "json encode: unsupport number key type." };
            }
            break;
        }
        default:
            throw std::logic_error{std::string("json encode: unsupport key type : ") + lua_typename(L, key_type)};
        }
        lua_pop(L, 1);
    }

    if (i == 0 && cfg->empty_as_array)
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

template <bool format>
static void encode_table_array(lua_State* L, size_t size, buffer* writer, int idx, int depth, json_config* cfg)
{
    size_t bsize = writer->size();
    writer->write_back('[');
    for (size_t i = 1; i <= size; i++)
    {
        if (i == 1)
            format_new_line<format>(writer);
        format_space<format>(writer, depth);
        lua_rawgeti(L, idx, i);
        if (lua_isnil(L, -1) && !cfg->enable_sparse_array){
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

template <bool format>
static void encode_table(lua_State* L, buffer* writer, int idx, int depth, json_config* cfg)
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
        encode_table_array<format>(L, size, writer, idx, depth, cfg);
    }
    else
    {
       encode_table_object<format>(L, writer, idx, depth, cfg);
    }
}

static int encode(lua_State* L)
{
    json_config* cfg = json_fetch_config(L);
    luaL_checkany(L, 1);

    try
    {
        buffer* writer = get_thread_encode_buffer();
        encode_one<false>(L, writer, 1, 0, cfg);
        lua_pushlstring(L, writer->data(), writer->size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        lua_pushstring(L, ex.what());
    }
    return lua_error(L);
}

static int pretty_encode(lua_State *L)
{
    json_config* cfg = json_fetch_config(L);
    luaL_checkany(L, 1);
    try
    {
        buffer* writer = get_thread_encode_buffer();
        lua_settop(L, 1);
        encode_one<true>(L, writer, 1, 0, cfg);
        lua_pushlstring(L, writer->data(), writer->size());
        return 1;
    }
    catch (const std::exception& ex)
    {
        lua_pushstring(L, ex.what());
    }
    return lua_error(L);
}

static void decode_one(lua_State* L, yyjson_val* value, json_config* cfg)
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
            decode_one(L, value, cfg);
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
                if ((c == '-' || (c >= '0' && c <= '9')) && cfg->enable_number_key)
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
                decode_one(L, val, cfg);
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
        lua_pushlightuserdata(L, nullptr);
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
    if (lua_type(L, 1) == LUA_TSTRING)
    {
        str = luaL_checklstring(L, 1, &len);
    }
    else
    {
        str = reinterpret_cast<const char*>(lua_touserdata(L, 1));
        len = luaL_checkinteger(L, 2);
    }

    if (nullptr == str || str[0] == '\0')
        return 0;

    json_config* cfg = json_fetch_config(L);

    lua_settop(L, 1);

    yyjson_read_err err;
    yyjson_doc* doc = yyjson_read_opts((char*)str, len, 0, (allocator.malloc) ? &allocator : nullptr, &err);
    if (nullptr == doc)
    {
        return luaL_error(L, "decode error: %s code: %d at position: %d\n", err.msg, (int)err.code, (int)err.pos);
    }
    decode_one(L, yyjson_doc_get_root(doc), cfg);
    yyjson_doc_free(doc);
    return 1;
}

static int concat(lua_State* L)
{
    if (lua_type(L, 1) == LUA_TSTRING)
    {
        size_t size;
        const char* sz = lua_tolstring(L, -1, &size);
        auto buf = new moon::buffer{size};
        buf->write_back(sz, size);
        lua_pushlightuserdata(L, buf);
        return 1;
    }

    luaL_checktype(L, 1, LUA_TTABLE);

    lua_settop(L, 1);

    json_config* cfg = json_fetch_config(L);

    auto buf = new moon::buffer(cfg->concat_buffer_size, cfg->concat_buffer_head_size);
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
                encode_one<false>(L, buf, -1, 0, cfg);
                break;
            }
            default:
                throw std::logic_error{std::string("json encode: unsupport value type :") +
                                       lua_typename(L, t)};
            }
            lua_pop(L, 1);
        }
        lua_pushlightuserdata(L, buf);
		return 1;
    }
    catch (const std::exception& ex)
    {
        delete buf;
        lua_pushstring(L, ex.what());
    }
    return lua_error(L);
}

static void write_resp(moon::buffer* buf, const char* cmd, size_t size)
{
    buf->write_back("\r\n$", 3);
    buf->write_chars(size);
    buf->write_back("\r\n", 2);
    buf->write_back(cmd, size);
}

static void concat_resp_one(moon::buffer* buf, lua_State* L, int i, json_config* cfg)
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
                concat_resp_one(buf, L, -1, cfg);
                lua_pop(L, 1);
            }
        }
        else
        {
            buffer* writer = get_thread_encode_buffer();
            encode_one<false>(L, writer, i, 0, cfg);
            write_resp(buf, writer->data(), writer->size());
        }
        break;
    }
    default:
        throw std::logic_error{std::string("json encode: unsupport value type :") + lua_typename(L, t)};
    }
}

static int concat_resp(lua_State* L)
{
    int n = lua_gettop(L);
    if (0 == n)
        return 0;

    json_config* cfg = json_fetch_config(L);

    auto buf = new moon::buffer(cfg->concat_buffer_size, cfg->concat_buffer_head_size);
    try
    {
        int64_t hash = 1;
        if(lua_type(L, 1) == LUA_TSTRING){
            size_t len = 0;
            const char* key = luaL_tolstring(L, 1, &len);
            if(len>0){
                std::string hash_part;
                if (n > 1) {
                    const char* field = luaL_tolstring(L, 2, &len);
                    if (len > 0)
                        hash_part.append(field, len);
                }

                if (n > 2 && (key[0] == 'h' || key[0] == 'H')) {
                    const char* field = luaL_tolstring(L, 3, &len);
                    if (len > 0)
                        hash_part.append(field, len);
                }
                hash = static_cast<uint32_t>(moon::hash_range(hash_part.begin(), hash_part.end()));
            }
        }

        buf->write_back('*');
        buf->write_chars(n);

        for (int i = 1; i <= n; i++)
        {
            concat_resp_one(buf, L, i, cfg);
        }

        buf->write_back("\r\n", 2);
        lua_pushlightuserdata(L, buf);
        lua_pushinteger(L, hash);
        return 2;
    }
    catch (const std::exception& ex)
    {
        delete buf;
        lua_pushstring(L, ex.what());
    }
    return lua_error(L);
}

extern "C" {
    int LUAMOD_API luaopen_json(lua_State* L)
    {
        luaL_Reg l[] = {
            { "encode", encode}, 
            { "pretty_encode", pretty_encode},
            { "decode", decode},
            { "concat", concat},
            { "concat_resp", concat_resp},
            { "options", json_options},
            { "null", nullptr},
            { nullptr, nullptr}
        };

        luaL_checkversion(L);
        luaL_newlibtable(L,l);
        json_create_config(L);
        luaL_setfuncs(L,l,1);

        lua_pushlightuserdata(L, nullptr);
        lua_setfield(L, -2, "null");
        return 1;
    }
}
