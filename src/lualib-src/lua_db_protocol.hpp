#pragma once

#include "common/buffer.hpp"
#include "common/byte_convert.hpp"
#include "common/hash.hpp"
#include "common/lua_utility.hpp"
#include "common/string.hpp"

using namespace moon;

/* These are the request codes sent by the frontend. */
constexpr char PQ_MSG_BIND = 'B';
constexpr char PQ_MSG_DESCRIBE = 'D';
constexpr char PQ_MSG_EXECUTE = 'E';
constexpr char PQ_MSG_PARSE = 'P';
constexpr char PQ_MSG_SYNC = 'S';

class lua_pq_error: public std::runtime_error {
public:
    using runtime_error::runtime_error;

    static lua_pq_error format(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        return lua_pq_error(buffer);
    }
};

class PQBuffer {
public:
    explicit PQBuffer(buffer* b): buf(b) {}

    void start_message(char msg_type) {
        buf->prepare(16);
        buf->unsafe_write_back(msg_type);
        stub = buf->size();
        buf->unsafe_write_back(0); // length placeholder
    }

    void end_message() noexcept {
        auto len = static_cast<uint32_t>(buf->size() - stub);
        moon::host2net(len);
        std::memcpy(buf->data() + stub, &len, sizeof(len));
    }

    template<typename T>
    void write_integer(T value) noexcept {
        moon::host2net(value);
        buf->write_back(value);
    }

    template<typename T>
    void write_back(T value) noexcept {
        buf->write_back(value);
    }

    template<typename T>
    void write_chars(T value) {
        buf->write_chars(value);
    }

    void write_null_terminated(std::string_view str) {
        buf->write_back(str);
        buf->write_back('\0');
    }

    buffer* get_buffer() noexcept {
        return buf;
    }

    size_t size() const noexcept {
        return buf->size();
    }

private:
    size_t stub = 0;
    buffer* buf = nullptr;
};

struct lua_db_protocol {
    static void write_lua_parameter(PQBuffer& pqbuf, lua_State* L, json_config* cfg) {
        int type = lua_type(L, -1);
        
        size_t stub = pqbuf.size();
        if (type != LUA_TNIL) {
            pqbuf.write_integer(static_cast<uint32_t>(0)); // length placeholder
        }
        
        switch (type) {
            case LUA_TNIL:
                pqbuf.write_integer(-1);
                break;
            case LUA_TNUMBER:
                if (lua_isinteger(L, -1)) {
                    pqbuf.write_chars(lua_tointeger(L, -1));
                } else {
                    pqbuf.write_chars(lua_tonumber(L, -1));
                }
                break;
            case LUA_TBOOLEAN:
                pqbuf.write_back(lua_toboolean(L, -1) ? "true" : "false");
                break;
            case LUA_TSTRING: {
                size_t sz = 0;
                const char* str = lua_tolstring(L, -1, &sz);
                pqbuf.write_back(std::string_view{ str, sz });
                break;
            }
            case LUA_TTABLE:
                encode_one<false>(L, pqbuf.get_buffer(), -1, 0, cfg);
                break;
            default:
                throw lua_pq_error::format(
                    "lua_pq_query: unsupported parameter type: %s",
                    lua_typename(L, type)
                );
        }
        
        if (type != LUA_TNIL) {
            auto buf_ptr = pqbuf.get_buffer();
            auto size = static_cast<uint32_t>(buf_ptr->size() - stub - 4);
            moon::host2net(size);
            std::memcpy(buf_ptr->data() + stub, &size, sizeof(size));
        }
    }

    static void append_pq_query(PQBuffer& pqbuf, std::string_view sql, lua_State* L, int index) {
        if (L != nullptr) {
            index = lua_absindex(L, index);
            lua_rawgeti(L, index, 1);
            if (lua_type(L, -1) != LUA_TSTRING) {
                lua_pop(L, 1);
                throw lua_pq_error::format("lua_pq_query: expected SQL string at index 1");
            }
            sql = moon::lua_check<std::string_view>(L, -1);
            lua_pop(L, 1);
        }

        /* construct the Parse message */
        pqbuf.start_message(PQ_MSG_PARSE);
        pqbuf.write_null_terminated("");
        pqbuf.write_null_terminated(sql);
        pqbuf.write_integer((uint16_t)0);
        pqbuf.end_message();

        /* construct the Bind message */
        pqbuf.start_message(PQ_MSG_BIND);
        pqbuf.write_null_terminated("");
        pqbuf.write_null_terminated("");

        uint32_t len = 1;
        if (L != nullptr) {
            len = static_cast<uint32_t>(lua_rawlen(L, index));
        }

        pqbuf.write_integer((uint16_t)0);
        pqbuf.write_integer((uint16_t)(len - 1)); // number of parameter formats

        if (L != nullptr) {
            json_config* cfg = json_fetch_config(L);

            for (uint32_t x = 2; x <= len; x++) {
                lua_rawgeti(L, index, x);
                write_lua_parameter(pqbuf, L, cfg);
                lua_pop(L, 1);
            }
        }

        pqbuf.write_integer((uint16_t)1);
        pqbuf.write_integer((uint16_t)0);
        pqbuf.end_message();

        /* construct the Describe Portal message */
        pqbuf.start_message(PQ_MSG_DESCRIBE);
        pqbuf.write_back('P');
        pqbuf.write_null_terminated("");
        pqbuf.end_message();

        /* construct the Execute message */
        pqbuf.start_message(PQ_MSG_EXECUTE);
        pqbuf.write_null_terminated("");
        pqbuf.write_integer((uint32_t)0);
        pqbuf.end_message();
    }

    static void append_pq_sync(PQBuffer& pqbuf) {
        pqbuf.start_message(PQ_MSG_SYNC);
        pqbuf.end_message();
    }

    static int pq_query(lua_State* L) {
        luaL_checktype(L, 1, LUA_TTABLE);
        try {
            auto buf = std::make_unique<moon::buffer>(256);
            PQBuffer pqbuf { buf.get() };
            append_pq_query(pqbuf, std::string_view {}, L, 1);
            append_pq_sync(pqbuf);
            lua_pushlightuserdata(L, buf.release());
            return 1;
        } catch (const lua_pq_error& ex) {
            lua_pushstring(L, ex.what());
        }
        return lua_error(L);
    }

    static int pq_pipe(lua_State* L) {
        luaL_checktype(L, 1, LUA_TTABLE);
        try {
            auto buf = std::make_unique<moon::buffer>(256);
            PQBuffer pqbuf { buf.get() };
            append_pq_query(pqbuf, "BEGIN", nullptr, 0);

            auto len = lua_rawlen(L, 1);
            for (int i = 0; i < len; i++) {
                lua_rawgeti(L, 1, i + 1);
                if (lua_type(L, -1) != LUA_TTABLE)
                    throw lua_pq_error::format("pq_transaction: expected table at index %d", i + 1);
                append_pq_query(pqbuf, std::string_view {}, L, -1);
                lua_pop(L, 1);
            }
            append_pq_query(pqbuf, "COMMIT", nullptr, 0);
            append_pq_sync(pqbuf);
            lua_pushlightuserdata(L, buf.release());
            return 1;
        } catch (const lua_pq_error& ex) {
            lua_pushstring(L, ex.what());
        }
        return lua_error(L);
    }

    static void concat_one_value(buffer* buf, lua_State* L, int index, json_config* cfg) {
        switch (int t = lua_type(L, index)) {
            case LUA_TNUMBER:
                if (lua_isinteger(L, index))
                    buf->write_chars(lua_tointeger(L, index));
                else
                    write_number(buf, lua_tonumber(L, index));
                break;
            case LUA_TBOOLEAN:
                buf->write_back(bool_string[lua_toboolean(L, index)]);
                break;
            case LUA_TSTRING: {
                size_t size;
                const char* sz = lua_tolstring(L, index, &size);
                buf->write_back({ sz, size });
                break;
            }
            case LUA_TTABLE:
                encode_one<false>(L, buf, index, 0, cfg);
                break;
            default:
                throw lua_json_error::format(
                    "json concat: unsupported value type: %s",
                    lua_typename(L, t)
                );
        }
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

        try {
            auto buf = std::make_unique<moon::buffer>(cfg->concat_buffer_size);
            buf->commit_unchecked(BUFFER_OPTION_CHEAP_PREPEND);
            auto array_size = static_cast<int>(lua_rawlen(L, 1));
            for (int i = 1; i <= array_size; i++) {
                lua_rawgeti(L, 1, i);
                concat_one_value(buf.get(), L, -1, cfg);
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

    static int64_t calculate_resp_hash(lua_State* L, int n) noexcept {
        if (lua_type(L, 2) != LUA_TTABLE)
            return 1;

        size_t len = 0;
        const char* key = lua_tolstring(L, 1, &len);
        if (len == 0)
            return 1;

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

        if (hash_part.empty())
            return 1;

        return static_cast<int64_t>(
            moon::hash_range(hash_part.begin(), hash_part.end())
        );
    }

    static int concat_resp(lua_State* L) {
        int n = lua_gettop(L);
        if (0 == n)
            return 0;

        json_config* cfg = json_fetch_config(L);

        try {
            auto buf = std::make_unique<moon::buffer>(cfg->concat_buffer_size);
            int64_t hash = calculate_resp_hash(L, n);

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
};
