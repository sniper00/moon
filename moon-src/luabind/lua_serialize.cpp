#include "lua.hpp"
#include "config.hpp"
#include "common/buffer.hpp"
#include "common/buffer_view.hpp"

using namespace moon;
static constexpr int32_t HEAP_BUFFER = 1;

#define TYPE_NIL 0
#define TYPE_BOOLEAN 1
// hibits 0 false 1 true
#define TYPE_NUMBER 2
// hibits 0 : 0 , 1: byte, 2:word, 4: dword, 6: qword, 8 : double
#define TYPE_NUMBER_ZERO 0
#define TYPE_NUMBER_BYTE 1
#define TYPE_NUMBER_WORD 2
#define TYPE_NUMBER_DWORD 4
#define TYPE_NUMBER_QWORD 6
#define TYPE_NUMBER_REAL 8

#define TYPE_USERDATA 3
#define TYPE_SHORT_STRING 4
// hibits 0~31 : len
#define TYPE_LONG_STRING 5
#define TYPE_TABLE 6

#define MAX_COOKIE 32
#define COMBINE_TYPE(t,v) ((t) | (v) << 3)

#define BLOCK_SIZE 128
#define MAX_DEPTH 32

static inline void wb_nil(buffer* buf)
{
    uint8_t n = TYPE_NIL;
    buf->write_back(&n, 1);
}

static inline void wb_boolean(buffer* buf, int boolean)
{
    uint8_t n = COMBINE_TYPE(TYPE_BOOLEAN, boolean ? 1 : 0);
    buf->write_back(&n, 1);
}

static inline void wb_integer(buffer* buf, lua_Integer v) {
    int type = TYPE_NUMBER;
    if (v == 0) {
        uint8_t n = (uint8_t)COMBINE_TYPE(type, TYPE_NUMBER_ZERO);
        buf->write_back(&n, 1);
    }
    else if (v != (int32_t)v) {
        uint8_t n = (uint8_t)COMBINE_TYPE(type, TYPE_NUMBER_QWORD);
        int64_t v64 = v;
        buf->write_back(&n, 1);
        buf->write_back(&v64, 1);
    }
    else if (v < 0) {
        int32_t v32 = (int32_t)v;
        uint8_t n = (uint8_t)COMBINE_TYPE(type, TYPE_NUMBER_DWORD);
        buf->write_back(&n, 1);
        buf->write_back(&v32, 1);
    }
    else if (v < 0x100) {
        uint8_t n = (uint8_t)COMBINE_TYPE(type, TYPE_NUMBER_BYTE);
        buf->write_back(&n, 1);
        uint8_t byte = (uint8_t)v;
        buf->write_back(&byte, 1);
    }
    else if (v < 0x10000) {
        uint8_t n = (uint8_t)COMBINE_TYPE(type, TYPE_NUMBER_WORD);
        buf->write_back(&n, 1);
        uint16_t word = (uint16_t)v;
        buf->write_back(&word, 1);
    }
    else {
        uint8_t n = (uint8_t)COMBINE_TYPE(type, TYPE_NUMBER_DWORD);
        buf->write_back(&n, 1);
        uint32_t v32 = (uint32_t)v;
        buf->write_back(&v32, 1);
    }
}

static inline void wb_real(buffer* buf, double v) {
    uint8_t n = COMBINE_TYPE(TYPE_NUMBER, TYPE_NUMBER_REAL);
    buf->write_back(&n, 1);
    buf->write_back(&v, 1);
}

static inline void wb_pointer(buffer* buf, void *v) {
    uint8_t n = TYPE_USERDATA;
    buf->write_back(&n, 1);
    buf->write_back(&v, 1);
}

static inline void wb_string(buffer* buf, const char *str, int len) {
    if (len < MAX_COOKIE) {
        uint8_t n = (uint8_t)COMBINE_TYPE(TYPE_SHORT_STRING, len);
        buf->write_back(&n, 1);
        if (len > 0) {
            buf->write_back(str, len);
        }
    }
    else {
        uint8_t n;
        if (len < 0x10000) {
            n = COMBINE_TYPE(TYPE_LONG_STRING, 2);
            buf->write_back(&n, 1);
            uint16_t x = (uint16_t)len;
            buf->write_back(&x, 1);
        }
        else {
            n = COMBINE_TYPE(TYPE_LONG_STRING, 4);
            buf->write_back(&n, 1);
            uint32_t x = (uint32_t)len;
            buf->write_back(&x, 1);
        }
        buf->write_back(str, len);
    }
}

static void pack_one(lua_State *L, buffer* b, int index, int depth);

static int wb_table_array(lua_State *L, buffer* buf, int index, int depth) {
    int array_size = (int)lua_rawlen(L, index);
    if (array_size >= MAX_COOKIE - 1) {
        uint8_t n = (uint8_t)COMBINE_TYPE(TYPE_TABLE, MAX_COOKIE - 1);
        buf->write_back(&n, 1);
        wb_integer(buf, array_size);
    }
    else {
        uint8_t n = (uint8_t)COMBINE_TYPE(TYPE_TABLE, array_size);
        buf->write_back(&n, 1);
    }

    int i;
    for (i = 1; i <= array_size; i++) {
        lua_rawgeti(L, index, i);
        pack_one(L, buf, -1, depth);
        lua_pop(L, 1);
    }

    return array_size;
}

static void wb_table_hash(lua_State *L, buffer* buf, int index, int depth, int array_size) {
    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        if (lua_type(L, -2) == LUA_TNUMBER) {
            if (lua_isinteger(L, -2)) {
                lua_Integer x = lua_tointeger(L, -2);
                if (x > 0 && x <= array_size) {
                    lua_pop(L, 1);
                    continue;
                }
            }
        }
        pack_one(L, buf, -2, depth);
        pack_one(L, buf, -1, depth);
        lua_pop(L, 1);
    }
    wb_nil(buf);
}

static void wb_table_metapairs(lua_State *L, buffer* buf, int index, int depth) {
    uint8_t n = COMBINE_TYPE(TYPE_TABLE, 0);
    buf->write_back(&n, 1);
    lua_pushvalue(L, index);
    lua_call(L, 1, 3);
    for (;;) {
        lua_pushvalue(L, -2);
        lua_pushvalue(L, -2);
        lua_copy(L, -5, -3);
        lua_call(L, 2, 2);
        int type = lua_type(L, -2);
        if (type == LUA_TNIL) {
            lua_pop(L, 4);
            break;
        }
        pack_one(L, buf, -2, depth);
        pack_one(L, buf, -1, depth);
        lua_pop(L, 1);
    }
    wb_nil(buf);
}

static void wb_table(lua_State*L, buffer* buf, int index, int depth)
{
    luaL_checkstack(L, LUA_MINSTACK, NULL);
    if (index < 0) {
        index = lua_gettop(L) + index + 1;
    }
    if (luaL_getmetafield(L, index, "__pairs") != LUA_TNIL) {
        wb_table_metapairs(L, buf, index, depth);
    }
    else {
        int array_size = wb_table_array(L, buf, index, depth);
        wb_table_hash(L, buf, index, depth, array_size);
    }
}

static void pack_one(lua_State *L, buffer* b, int index, int depth) {
    if (depth > MAX_DEPTH) {
        if (b->has_flag(HEAP_BUFFER)) delete b;
        luaL_error(L, "serialize can't pack too depth table");
        return;
    }
    int type = lua_type(L, index);
    switch (type) {
    case LUA_TNIL:
        wb_nil(b);
        break;
    case LUA_TNUMBER: {
        if (lua_isinteger(L, index)) {
            lua_Integer x = lua_tointeger(L, index);
            wb_integer(b, x);
        }
        else {
            lua_Number n = lua_tonumber(L, index);
            wb_real(b, n);
        }
        break;
    }
    case LUA_TBOOLEAN:
        wb_boolean(b, lua_toboolean(L, index));
        break;
    case LUA_TSTRING: {
        size_t sz = 0;
        const char *str = lua_tolstring(L, index, &sz);
        wb_string(b, str, (int)sz);
        break;
    }
    case LUA_TLIGHTUSERDATA:
        wb_pointer(b, lua_touserdata(L, index));
        break;
    case LUA_TTABLE: {
        if (index < 0) {
            index = lua_gettop(L) + index + 1;
        }
        wb_table(L, b, index, depth + 1);
        break;
    }
    default:
        if (b->has_flag(HEAP_BUFFER)) delete b;
        luaL_error(L, "Unsupport type %s to serialize", lua_typename(L, type));
    }
}

static void invalid_stream_line(lua_State *L, buffer_view* buf, int line) {
    int len = (int)buf->size();
    luaL_error(L, "Invalid serialize stream %d (line:%d)", len, line);
}

#define invalid_stream(L,rb) invalid_stream_line(L,rb,__LINE__)

static lua_Integer get_integer(lua_State *L, buffer_view* buf, int cookie) {
    switch (cookie) {
    case TYPE_NUMBER_ZERO:
        return 0;
    case TYPE_NUMBER_BYTE: {
        uint8_t n{};
        if (!buf->read(&n))
            invalid_stream(L, buf);
        return n;
    }
    case TYPE_NUMBER_WORD: {
        uint16_t n{};
        if (!buf->read(&n))
            invalid_stream(L, buf);
        return n;
    }
    case TYPE_NUMBER_DWORD: {
        int32_t n{};
        if (!buf->read(&n))
            invalid_stream(L, buf);
        return n;
    }
    case TYPE_NUMBER_QWORD: {
        int64_t n{};
        if (!buf->read(&n))
            invalid_stream(L, buf);
        return n;
    }
    default:
        invalid_stream(L, buf);
        return 0;
    }
}

static double get_real(lua_State *L, buffer_view* buf) {
    double n;
    if (!buf->read(&n))
        invalid_stream(L, buf);
    return n;
}

static void * get_pointer(lua_State *L, buffer_view* buf) {
    void * userdata = 0;
    if (!buf->read(&userdata))
        invalid_stream(L, buf);
    return userdata;
}

static void get_buffer(lua_State *L, buffer_view* buf, int len) {
    if (int(buf->size()) < len) {
        invalid_stream(L, buf);
    }
    lua_pushlstring(L, buf->data(), len);
    buf->skip(len);
}

static void unpack_one(lua_State *L, buffer_view* buf);

static void unpack_table(lua_State *L, buffer_view* buf, int array_size) {
    if (array_size == MAX_COOKIE - 1) {
        uint8_t type{};
        if (!buf->read(&type))
            invalid_stream(L, buf);
        int cookie = type >> 3;
        if ((type & 7) != TYPE_NUMBER || cookie == TYPE_NUMBER_REAL) {
            invalid_stream(L, buf);
        }
        array_size = (int)get_integer(L, buf, cookie);
    }
    luaL_checkstack(L, LUA_MINSTACK, NULL);
    lua_createtable(L, array_size, 0);
    int i;
    for (i = 1; i <= array_size; i++) {
        unpack_one(L, buf);
        lua_rawseti(L, -2, i);
    }
    for (;;) {
        unpack_one(L, buf);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            return;
        }
        unpack_one(L, buf);
        lua_rawset(L, -3);
    }
}

static void push_value(lua_State *L, buffer_view* buf, int type, int cookie)
{
    switch (type) {
    case TYPE_NIL:
        lua_pushnil(L);
        break;
    case TYPE_BOOLEAN:
        lua_pushboolean(L, cookie);
        break;
    case TYPE_NUMBER:
        if (cookie == TYPE_NUMBER_REAL) {
            lua_pushnumber(L, get_real(L, buf));
        }
        else {
            lua_pushinteger(L, get_integer(L, buf, cookie));
        }
        break;
    case TYPE_USERDATA:
        lua_pushlightuserdata(L, get_pointer(L, buf));
        break;
    case TYPE_SHORT_STRING:
        get_buffer(L, buf, cookie);
        break;
    case TYPE_LONG_STRING: {
        if (cookie == 2) {
            uint16_t n{};
            if (!buf->read(&n))
                invalid_stream(L, buf);
            get_buffer(L, buf, n);
        }
        else {
            if (cookie != 4) {
                invalid_stream(L, buf);
            }
            uint32_t n{};
            if (!buf->read(&n))
                invalid_stream(L, buf);
            get_buffer(L, buf, n);
        }
        break;
    }
    case TYPE_TABLE: {
        unpack_table(L, buf, cookie);
        break;
    }
    default: {
        invalid_stream(L, buf);
        break;
    }
    }
}

static void unpack_one(lua_State *L, buffer_view* buf) {
    uint8_t type{};
    if (!buf->read(&type))
        invalid_stream(L, buf);
    push_value(L, buf, type & 0x7, type >> 3);
}

static int pack(lua_State* L)
{
    int n = lua_gettop(L);
    if (0 == n)
    {
        return 0;
    }
    auto buf = new buffer(64, BUFFER_HEAD_RESERVED);
    buf->set_flag(HEAP_BUFFER);

    for (int i = 1; i <= n; i++) {
        pack_one(L, buf, i, 0);
    }
    buf->clear_flag(HEAP_BUFFER);
    lua_pushlightuserdata(L, buf);
    return 1;
}

static int packsafe(lua_State* L)
{
    int n = lua_gettop(L);
    if (0 == n)
    {
        return 0;
    }

    buffer buf;
    for (int i = 1; i <= n; i++) {
        pack_one(L, &buf, i, 0);
    }
    lua_pushlstring(L, buf.data(), buf.size());
    return 1;
}

static int unpack(lua_State* L)
{
    if (lua_isnoneornil(L, 1)) {
        return 0;
    }
    const char* data;
    size_t len;
    if (lua_type(L, 1) == LUA_TSTRING) {
        data = lua_tolstring(L, 1, &len);
    }
    else
    {
        data = (const char*)lua_touserdata(L, 1);
        len = luaL_checkinteger(L, 2);
    }

    if (len == 0) {
        return 0;
    }

    if (data == NULL) {
        return luaL_error(L, "deserialize null pointer");
    }

    lua_settop(L, 1);
    buffer_view br(data, len);

    for (int i = 0;; i++)
    {
        if (i % 8 == 7)
        {
            luaL_checkstack(L, LUA_MINSTACK, NULL);
        }
        uint8_t type = 0;
        if (!br.read(&type))
        {
            break;
        }
        push_value(L, &br, type & 0x7, type >> 3);
    }
    return lua_gettop(L) - 1;
}

static int peek_one(lua_State* L)
{
    if (lua_isnoneornil(L, 1)) {
        return 0;
    }

    if (lua_type(L, 1) != LUA_TLIGHTUSERDATA)
    {
        return luaL_error(L, "need userdata");
    }

    int seek = 0;

    if (lua_type(L, 2) == LUA_TBOOLEAN)
    {
        seek = lua_toboolean(L, 2);
    }

    buffer* buf = (buffer*)lua_touserdata(L, 1);
    buffer_view br(buf->data(), buf->size());

    uint8_t type = 0;
    if (!br.read(&type))
    {
        return 0;
    }

    push_value(L, &br, type & 0x7, type >> 3);

    if (seek)
    {
        assert(!buf->has_flag(buffer_flag::broadcast));
        buf->seek(static_cast<int>(buf->size() - br.size()));
    }
    lua_pushlightuserdata(L, (void*)br.data());
    lua_pushinteger(L, static_cast<int64_t>(br.size()));
    return 3;
}

static void concat_one(lua_State *L, buffer* b, int index, int depth);

static int concat_table_array(lua_State *L, buffer* buf, int index, int depth) {
    int array_size = (int)lua_rawlen(L, index);
    int i;
    for (i = 1; i <= array_size; i++) {
        lua_rawgeti(L, index, i);
        concat_one(L, buf, -1, depth);
        lua_pop(L, 1);
    }
    return array_size;
}

static void concat_table(lua_State*L, buffer* buf, int index, int depth)
{
    luaL_checkstack(L, LUA_MINSTACK, NULL);
    if (index < 0) {
        index = lua_gettop(L) + index + 1;
    }
    concat_table_array(L, buf, index, depth);
}

static void concat_one(lua_State *L, buffer* b, int index, int depth)
{
    if (depth > MAX_DEPTH) {
        if (b->has_flag(HEAP_BUFFER)) delete b;
        luaL_error(L, "serialize can't concat too depth table");
        return;
    }
    int type = lua_type(L, index);
    switch (type) {
    case LUA_TNIL:
        break;
    case LUA_TNUMBER:
    {
        if (lua_isinteger(L, index))
        {
            lua_Integer x = lua_tointeger(L, index);
            auto s = std::to_string(x);
            b->write_back(s.data(), s.size());
        }
        else
        {
            lua_Number n = lua_tonumber(L, index);
            auto s = std::to_string(n);
            b->write_back(s.data(), s.size());
        }
        break;
    }
    case LUA_TBOOLEAN:
    {
        int n = lua_toboolean(L, index);
        const char* s = n ? "true" : "false";
        b->write_back(s, std::strlen(s));
        break;
    }
    case LUA_TSTRING: {
        size_t sz = 0;
        const char *str = lua_tolstring(L, index, &sz);
        b->write_back(str, sz);
        break;
    }
    case LUA_TTABLE: {
        if (index < 0)
        {
            index = lua_gettop(L) + index + 1;
        }
        concat_table(L, b, index, depth + 1);
        break;
    }
    default:
        if (b->has_flag(HEAP_BUFFER)) delete b;
        luaL_error(L, "Unsupport type %s to concat", lua_typename(L, type));
    }
}

static int concat(lua_State* L)
{
    int n = lua_gettop(L);
    if (0 == n)
    {
        return 0;
    }
    auto buf = new buffer(64, BUFFER_HEAD_RESERVED);
    buf->set_flag(HEAP_BUFFER);
    for (int i = 1; i <= n; i++) {
        concat_one(L, buf, i, 0);
    }
    buf->clear_flag(HEAP_BUFFER);
    lua_pushlightuserdata(L, buf);
    return 1;
}

static int concatsafe(lua_State *L)
{
    int n = lua_gettop(L);
    if (0 == n)
    {
        return 0;
    }

    buffer buf;
    for (int i = 1; i <= n; i++) {
        concat_one(L, &buf, i, 0);
    }
    lua_pushlstring(L, buf.data(), buf.size());
    return 1;
}

static int sep_concat(lua_State* L)
{
    int n = lua_gettop(L);
    if (n < 1)
    {
        return 0;
    }

    size_t len;
    const char* sep = luaL_checklstring(L, 1, &len);
    auto buf = new buffer(64, BUFFER_HEAD_RESERVED);
    buf->set_flag(HEAP_BUFFER);
    for (int i = 2; i <= n; i++) {
        concat_one(L, buf, i, 0);
        if(i!=n)
            buf->write_back(sep, len);
    }
    buf->clear_flag(HEAP_BUFFER);
    lua_pushlightuserdata(L, buf);
    return 1;
}

static int sep_concatsafe(lua_State *L)
{
    int n = lua_gettop(L);
    if (n < 1)
    {
        return 0;
    }

    size_t len;
    const char* sep = luaL_checklstring(L, 1, &len);

    buffer buf;
    for (int i = 2; i <= n; i++) {
        concat_one(L, &buf, i, 0);
        if (i != n)
            buf.write_back(sep, len);
    }
    lua_pushlstring(L, buf.data(), buf.size());
    return 1;
}

extern "C"
{
    int LUAMOD_API  luaopen_serialize(lua_State *L)
    {
        luaL_checkversion(L);
        luaL_Reg l[] = {
            {"pack",pack},
            {"packs",packsafe },
            {"unpack",unpack},
            {"unpack_one",peek_one},
            {"concat",concat },
            {"concats",concatsafe },
            {"sep_concat",sep_concat },
            {"sep_concats",sep_concatsafe },
            {NULL,NULL},
        };
        luaL_newlib(L, l);
        return 1;
    }
}

