#include "lua.hpp"
#include <vector>
#include <charconv>
#include <string_view>

// __SSE2__ and __SSE4_2__ are recognized by gcc, clang, and the Intel compiler.
// We use -march=native with gmake to enable -msse2 and -msse4.2, if supported.
#if defined(__SSE4_2__)
#  define RAPIDJSON_SSE42
#elif defined(__SSE2__)
#  define RAPIDJSON_SSE2
#endif

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/reader.h"
#include "rapidjson/error/en.h"

static constexpr int max_depth = 64;

using StreamBuf = rapidjson::StringBuffer;
using JsonWriter = rapidjson::Writer<StreamBuf>;
using JsonPrettyWriter = rapidjson::PrettyWriter<StreamBuf>;

using JsonReader = rapidjson::Reader;

namespace rapidjson
{
    // StringNStream
    //! Read-only string stream.
    /*! \note implements Stream concept
    */
    template <typename Encoding>
    struct GenericStringNStream {
        typedef typename Encoding::Ch Ch;

        GenericStringNStream(const Ch* src, const size_t len) : src_(src), head_(src), len_(len) {}

        Ch Peek() const { return Tell() < len_ ? *src_ : '\0'; }
        Ch Take() { return *src_++; }
        size_t Tell() const { return static_cast<size_t>(src_ - head_); }

        Ch* PutBegin() { RAPIDJSON_ASSERT(false); return 0; }
        void Put(Ch) { RAPIDJSON_ASSERT(false); }
        void Flush() { RAPIDJSON_ASSERT(false); }
        size_t PutEnd(Ch*) { RAPIDJSON_ASSERT(false); return 0; }

        const Ch* src_;     //!< Current read position.
        const Ch* head_;    //!< Original head of the string.
        size_t len_;    //!< Original head of the string.
    };
    //! String stream with UTF8 encoding.
    typedef GenericStringNStream<UTF8<> > StringNStream;

    template <typename Encoding>
    struct StreamTraits<GenericStringNStream<Encoding> > {
        enum { copyOptimization = 1 };
    };
}

using StringNStream = rapidjson::StringNStream;

template<typename Writer>
static void encode_table(lua_State* L, Writer* writer, int idx, int depth);

template<typename Writer>
static void encode_one(lua_State* L, Writer* writer, int idx, int depth = 0)
{
    int t = lua_type(L, idx);
    switch (t)
    {
    case LUA_TBOOLEAN:
    {
        writer->Bool(lua_toboolean(L, idx) != 0);
        return;
    }
    case LUA_TNUMBER:
    {
        if (lua_isinteger(L, idx))
        {
            writer->Int64(lua_tointeger(L, idx));
        }
        else
        {
            if (!writer->Double(lua_tonumber(L, idx)))
                luaL_error(L, "error while encode double value.");
        }
        return;
    }
    case LUA_TSTRING:
    {
        size_t len = 0;
        const char* str = lua_tolstring(L, idx, &len);
        writer->String(str, static_cast<rapidjson::SizeType>(len));
        return;
    }
    case LUA_TTABLE:
    {
        encode_table(L, writer, idx, depth + 1);
        return;
    }
    case LUA_TNIL:
    {
        writer->Null();
        return;
    }
    default:
        luaL_error(L, "json encode: unsupport value type : %s", lua_typename(L, t));
    }
}

static inline size_t array_size(lua_State* L, int idx)
{
    if (lua_getmetatable(L, idx)) {
        // [metatable]
        lua_getfield(L, -1, "__jobject");
        auto v = lua_isnoneornil(L, -1);
        lua_pop(L, 2);
        if (!v)
        {
            return 0;
        }
    }
    return lua_rawlen(L, idx);
}

template<typename Writer>
static void encode_table(lua_State* L, Writer* writer, int idx, int depth)
{
    if (depth > max_depth)
        luaL_error(L, "nested too depth");

    if (idx < 0) {
        idx = lua_gettop(L) + idx + 1;
    }

    luaL_checkstack(L, LUA_MINSTACK, NULL);

    if (size_t size = array_size(L, idx); size >0)
    {
        writer->StartArray();
        for (size_t i = 1; i <= size; i++)
        {
            lua_rawgeti(L, idx, i);
            encode_one(L, writer, -1, depth);
            lua_pop(L, 1);
        }
        writer->EndArray();
    }
    else
    {
        writer->StartObject();
        lua_pushnil(L);
        while (lua_next(L, -2))
        {
            int key_type = lua_type(L, -2);
            switch (key_type)
            {
            case LUA_TSTRING:
            {
                size_t len = 0;
                const char* key = lua_tolstring(L, -2, &len);
                writer->Key(key, static_cast<rapidjson::SizeType>(len));
                encode_one(L, writer, -1, depth);
                break;
            }
            case LUA_TNUMBER:
            {
                lua_Integer key = luaL_checkinteger(L, -2);
                char tmp[256];
                auto res = std::to_chars(tmp, tmp + 255, key);
                if (res.ec != std::errc())
                {
                    luaL_error(L, "json encode: int key to_chars failed.");
                }
                writer->Key(tmp, static_cast<rapidjson::SizeType>(res.ptr - tmp), true);
                encode_one(L, writer, -1, depth);
                break;
            }
            default:
                luaL_error(L, "json encode: object only support string and integer key");
                break;
            }
            lua_pop(L, 1);
        }
        writer->EndObject();
    }
}

static int  encode(lua_State* L)
{
    StreamBuf stream;
    JsonWriter writer(stream);
    encode_one(L, &writer, -1);
    lua_pushlstring(L, stream.GetString(), stream.GetSize());
    return 1;
}

static int  pretty_encode(lua_State* L)
{
    StreamBuf stream;
    JsonPrettyWriter writer(stream);
    encode_one(L, &writer, -1);
    lua_pushlstring(L, stream.GetString(), stream.GetSize());
    return 1;
}

struct LuaPushHandler {
    explicit LuaPushHandler(lua_State* aL) : L(aL) {}

    bool Null() {
        static int json_null = LUA_NOREF;
        lua_rawgeti(L, LUA_REGISTRYINDEX, json_null);
        Commit();
        return true;
    }

    bool Bool(bool b) {
        lua_pushboolean(L, b);
        Commit();
        return true;
    }

    bool Int(int i) {
        lua_pushinteger(L, i);
        Commit();
        return true;
    }

    bool Uint(unsigned u) {
        lua_pushinteger(L, u);
        Commit();
        return true;
    }

    bool Int64(int64_t i) {
        lua_pushinteger(L, i);
        Commit();
        return true;
    }

    bool Uint64(uint64_t u) {
        lua_pushinteger(L, u);
        Commit();
        return true;
    }

    bool Double(double d) {
        lua_pushnumber(L, static_cast<lua_Number>(d));
        Commit();
        return true;
    }

    bool RawNumber(const char* str, rapidjson::SizeType length, bool /*copy*/) {
        lua_getglobal(L, "tonumber");
        lua_pushlstring(L, str, length);
        lua_call(L, 1, 1);
        Commit();
        return true;
    }

    bool String(const char* str, rapidjson::SizeType length, bool /*copy*/) {
        lua_pushlstring(L, str, length);
        Commit();
        return true;
    }

    bool StartObject() {
        luaL_checkstack(L, LUA_MINSTACK, NULL);
        lua_createtable(L, 0, 8);
        stack_.push_back(0);// 0 means hash, >0 means array
        return true;
    }

    bool Key(const char* str, rapidjson::SizeType length, bool /*copy*/) const {
        if (length == 0)
        {
            return false;
        }
        char c = str[0];
        if (c == '-' || (c >= '0' && c <= '9'))
        {
            int64_t v = 0;
            auto [p, ec] = std::from_chars(str, str + length, v);
            if (ec != std::errc())
            {
                return false;
            }
            lua_pushinteger(L, v);
        }
        else
        {
            lua_pushlstring(L, str, length);
        }
        return true;
    }

    bool EndObject(rapidjson::SizeType /*memberCount*/) {
        stack_.pop_back();
        Commit();
        return true;
    }

    bool StartArray() {
        luaL_checkstack(L, LUA_MINSTACK, NULL);
        lua_createtable(L, 8, 0);
        stack_.push_back(1);
        return true;
    }

    bool EndArray(rapidjson::SizeType /*elementCount*/) {
        stack_.pop_back();
        Commit();
        return true;
    }

private:
    void Commit()
    {
        if (!stack_.empty())
        {
            //obj
            if (stack_.back() == 0)
            {
                lua_rawset(L, -3);
            }
            else
            {
                lua_rawseti(L, -2, stack_.back()++);
            }
        }
    }
    lua_State* L;
    std::vector<int> stack_;
};

static int decode(lua_State* L)
{
    size_t len = 0;
    const char* str = nullptr;
    if (lua_type(L, 1) == LUA_TSTRING) {
        str = luaL_checklstring(L, 1, &len);
    }
    else {
        str = reinterpret_cast<const char*>(lua_touserdata(L, 1));
        len = luaL_checkinteger(L, 2);
    }

    if (nullptr == str)
    {
        return 0;
    }

    lua_settop(L, 1);
    JsonReader reader;
    StringNStream stream{ str, len };
    LuaPushHandler handler{ L };
    auto res = reader.Parse(stream, handler);
    if (!res) {
        lua_settop(L, 1);
        lua_pushnil(L);
        lua_pushfstring(L, "%s (%d)", rapidjson::GetParseError_En(res.Code()), res.Offset());
    }
    return lua_gettop(L) - 1;
}

static int newobject(lua_State* L)
{
    int n = (int)luaL_optinteger(L, 1, 7);
    lua_createtable(L, 0, n);
    if (luaL_newmetatable(L, "json_object_mt"))//mt
    {
        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "__jobject");//
    }
    lua_setmetatable(L, -2);// set userdata metatable
    return 1;
}

extern "C"
{
    int LUAMOD_API luaopen_json(lua_State* L)
    {
        luaL_Reg l[] = {
            {"encode", encode},
            {"pretty_encode", pretty_encode},
            {"decode", decode},
            {"newobject", newobject},
            {NULL,NULL}
        };
        luaL_checkversion(L);
        luaL_newlib(L, l);
        return 1;
    }
}
