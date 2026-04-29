#include "common/buffer.hpp"
#include "common/byte_convert.hpp"
#include "common/lua_utility.hpp"
#include "common/string.hpp"
#include "core/config.hpp"
#include "lua.hpp"

using namespace moon;

constexpr int MAX_DEPTH = 32;

static buffer* get_pointer(lua_State* L, int index) {
    buffer* b = nullptr;
    int tp = lua_type(L, index);
    if (tp == LUA_TLIGHTUSERDATA) {
        b = static_cast<buffer*>(lua_touserdata(L, index));
    } else if (tp == LUA_TUSERDATA) {
        buffer_shr_ptr_t* shr = static_cast<buffer_shr_ptr_t*>(lua_touserdata(L, index));
        if (shr == nullptr) {
            luaL_argerror(L, index, "buffer: expected buffer_shr_ptr_t userdata, got null pointer");
            return nullptr;
        }
        b = shr->get();
    } else {
        moon::lua_argferror(
            L,
            index,
            "buffer: expected buffer lightuserdata or buffer_shr_ptr_t userdata, got %s",
            lua_typename(L, tp)
        );
        return nullptr;
    }

    if (b == nullptr) {
        luaL_argerror(L, index, "buffer: expected valid buffer pointer, got null pointer");
    }
    return b;
}

static int clear(lua_State* L) {
    auto* buf = get_pointer(L, 1);
    buf->clear();
    return 0;
}

static int size(lua_State* L) {
    auto* buf = get_pointer(L, 1);
    lua_pushinteger(L, buf->size());
    return 1;
}

template<typename T>
static void push_integer(lua_State* L, const char*& b, const char* e, bool little) {
    if (ptrdiff_t len = e - b; len < static_cast<ptrdiff_t>(sizeof(T))) {
        luaL_error(
            L,
            "buffer.unpack: insufficient data for %I-byte integer, need %I bytes, got %I bytes",
            (lua_Integer)sizeof(T),
            (lua_Integer)sizeof(T),
            (lua_Integer)len
        );
    }

    T v = 0;
    memcpy(&v, b, sizeof(T));
    b += sizeof(T);
    if (!little) {
        moon::host2net(v);
    }
    lua_pushinteger(L, v);
}

static void check_unpack_position(lua_State* L, int arg, size_t pos, size_t buf_size) {
    if (pos > buf_size) {
        moon::lua_argferror(
            L,
            arg,
            "buffer.unpack: position out of range (pos=%I, buffer_size=%I)",
            (lua_Integer)pos,
            (lua_Integer)buf_size
        );
    }
}

static int unpack(lua_State* L) {
    auto* buf = get_pointer(L, 1);

    int top = lua_gettop(L);

    if (int tp = lua_type(L, 2); tp == LUA_TSTRING) {
        size_t opt_len = 0;
        const char* opt = luaL_optlstring(L, 2, "", &opt_len);
        auto pos = static_cast<size_t>(luaL_optinteger(L, 3, 0));
        check_unpack_position(L, 3, pos, buf->size());

        const char* start = buf->data() + pos;
        const char* end = buf->data() + buf->size();

        bool little = true;
        for (size_t i = 0; i < opt_len; ++i) {
            switch (opt[i]) {
                case '>':
                    little = false;
                    break;
                case '<':
                    little = true;
                    break;
                case 'h':
                    push_integer<int16_t>(L, start, end, little);
                    break;
                case 'H':
                    push_integer<uint16_t>(L, start, end, little);
                    break;
                case 'i':
                    push_integer<int32_t>(L, start, end, little);
                    break;
                case 'I':
                    push_integer<uint32_t>(L, start, end, little);
                    break;
                case 'C':
                    lua_pushlightuserdata(L, (void*)start);
                    lua_pushinteger(L, end - start);
                    start = end;
                    break;
                default:
                    return moon::lua_argferror(
                        L,
                        2,
                        "buffer.unpack: invalid format character '%c', valid options are: '>' (big-endian), '<' (little-endian), 'h' (int16), 'H' (uint16), 'i' (int32), 'I' (uint32), 'C' (raw data)",
                        (int)opt[i]
                    );
            }
        }
    } else {
        auto pos = static_cast<size_t>(luaL_optinteger(L, 2, 0));
        check_unpack_position(L, 2, pos, buf->size());

        lua_Integer count_arg = luaL_optinteger(L, 3, -1);
        size_t count;
        if (count_arg < 0) {
            count = buf->size() - pos;
        } else {
            count = static_cast<size_t>(count_arg);
            count = std::min(buf->size() - pos, count);
        }
        lua_pushlstring(L, buf->data() + pos, count);
    }
    return lua_gettop(L) - top;
}

template<typename T>
static void pack_integer(buffer* buf, lua_State* L, int arg, bool little) {
    auto v = static_cast<T>(luaL_checkinteger(L, arg));
    if (!little) {
        moon::host2net(v);
    }
    buf->write_back(v);
}

extern void serialize_pack_one(lua_State* L, buffer* b, int index, int depth);

static int pack(lua_State* L) {
    int n = lua_gettop(L);
    if (0 == n) {
        return 0;
    }

    size_t opt_len = 0;
    const char* opt = luaL_checklstring(L, 1, &opt_len);

    auto buf = std::make_unique<moon::buffer>(256);
    buf->commit_unchecked(BUFFER_OPTION_CHEAP_PREPEND);

    bool little = true;
    int arg = 2;
    for (size_t i = 0; i < opt_len; ++i) {
        switch (opt[i]) {
            case '>':
                little = false;
                continue;
            case '<':
                little = true;
                continue;
            default:
                break;
        }

        if (arg > n) {
            return luaL_error(
                L,
                "buffer.pack: missing argument for format '%c' at position %d",
                opt[i],
                (int)i + 1
            );
        }

        switch (opt[i]) {
            case 'h': {
                pack_integer<int16_t>(buf.get(), L, arg++, little);
                break;
            }
            case 'H': {
                pack_integer<uint16_t>(buf.get(), L, arg++, little);
                break;
            }
            case 'i': {
                pack_integer<int32_t>(buf.get(), L, arg++, little);
                break;
            }
            case 'I': {
                pack_integer<uint32_t>(buf.get(), L, arg++, little);
                break;
            }
            case 'S': {
                serialize_pack_one(L, buf.get(), arg++, 0);
                break;
            }
            case 'C': {
                if (arg + 1 > n) {
                    return luaL_error(
                        L,
                        "buffer.pack: missing argument for format 'C' at position %d",
                        (int)i + 1
                    );
                }
                luaL_checktype(L, arg, LUA_TLIGHTUSERDATA);
                const char* str = reinterpret_cast<const char*>(lua_touserdata(L, arg++));
                size_t len = luaL_checkinteger(L, arg++);
                buf->write_back({ str, len });
                break;
            }
            default:
                return moon::lua_argferror(
                    L,
                    1,
                    "buffer.pack: invalid format character '%c', valid options are: '>' (big-endian), '<' (little-endian), 'h' (int16), 'H' (uint16), 'i' (int32), 'I' (uint32), 'C' (raw string)",
                    (int)opt[i]
                );
        }
    }
    buf->consume_unchecked(BUFFER_OPTION_CHEAP_PREPEND);
    lua_pushlightuserdata(L, buf.release());
    return 1;
}

static int read(lua_State* L) {
    auto* buf = get_pointer(L, 1);
    auto count = static_cast<size_t>(luaL_checkinteger(L, 2));
    if (count > buf->size()) {
        return moon::lua_argferror(
            L,
            2,
            "buffer.read: requested %I bytes but buffer only has %I bytes",
            (lua_Integer)count,
            (lua_Integer)buf->size()
        );
    }
    lua_pushlstring(L, buf->data(), count);
    buf->consume_unchecked(count);
    return 1;
}

static int write_one(lua_State* L, buffer* b, int index, int depth);

static int write_table_array(lua_State* L, buffer* buf, int index, int depth) {
    luaL_checkstack(L, LUA_MINSTACK, nullptr);
    index = lua_absindex(L, index);
    int array_size = (int)lua_rawlen(L, index);
    int i;
    for (i = 1; i <= array_size; i++) {
        lua_rawgeti(L, index, i);
        write_one(L, buf, -1, depth);
        lua_pop(L, 1);
    }
    return array_size;
}

static int write_one(lua_State* L, buffer* b, int index, int depth) {
    if (depth > MAX_DEPTH) {
        throw std::logic_error { "buffer.concat: table nesting too deep (max "
                                 + std::to_string(MAX_DEPTH)
                                 + " levels), possible circular reference" };
    }

    int type = lua_type(L, index);
    switch (type) {
        case LUA_TNIL:
            return 1;
        case LUA_TNUMBER: {
            lua_isinteger(L, index) != 0 ? b->write_chars(lua_tointeger(L, index))
                                         : b->write_chars(lua_tonumber(L, index));
            return 1;
        }
        case LUA_TBOOLEAN: {
            constexpr std::string_view bool_string[2] = { "false", "true" };
            b->write_back(bool_string[lua_toboolean(L, index)]);
            return 1;
        }
        case LUA_TSTRING: {
            size_t sz = 0;
            const char* str = lua_tolstring(L, index, &sz);
            b->write_back({ str, sz });
            return 1;
        }
        case LUA_TTABLE: {
            write_table_array(L, b, index, depth + 1);
            return 1;
        }
        case LUA_TLIGHTUSERDATA: {
            const char* str = reinterpret_cast<const char*>(lua_touserdata(L, index));
            size_t len = luaL_checkinteger(L, index + 1);
            b->write_back({ str, len });
            return 2;
        }
        default:
            throw std::logic_error {
                "buffer.concat: unsupported type '" + std::string(lua_typename(L, type))
                + "', only string, number, boolean, table, nil, and lightuserdata(const char*) are supported"
            };
    }
}

static int write_front(lua_State* L) {
    auto* buf = get_pointer(L, 1);
    int top = lua_gettop(L);
    for (int i = top; i > 1; --i) {
        size_t len = 0;
        const auto* data = luaL_checklstring(L, i, &len);
        if (!buf->write_front(data, len)) {
            luaL_argerror(L, 1, "buffer.write_front: total prepend size exceeds limit");
        }
    }
    return 0;
}

static int write_back(lua_State* L) {
    auto* buf = get_pointer(L, 1);
    try {
        int n = lua_gettop(L);
        for (int i = 2; i <= n;) {
            i += write_one(L, buf, i, 0);
        }
        return 0;
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

static int seek(lua_State* L) {
    auto* buf = get_pointer(L, 1);
    auto pos = static_cast<size_t>(luaL_checkinteger(L, 2));
    auto origin =
        ((luaL_optinteger(L, 3, 1) == 1) ? buffer::seek_origin::Current
                                         : buffer::seek_origin::Begin);
    if (!buf->seek(pos, origin)) {
        return moon::lua_argferror(
            L,
            2,
            "buffer.seek: position out of range (pos=%I, buffer_size=%I)",
            (lua_Integer)pos,
            (lua_Integer)buf->size()
        );
    }
    return 0;
}

static int commit(lua_State* L) {
    auto* buf = get_pointer(L, 1);
    auto n = static_cast<size_t>(luaL_checkinteger(L, 2));
    if (!buf->commit(n)) {
        return moon::lua_argferror(
            L,
            2,
            "buffer.commit: commit size %I exceeds prepared capacity",
            (lua_Integer)n
        );
    }
    return 0;
}

static int prepare(lua_State* L) {
    auto* buf = get_pointer(L, 1);
    auto n = static_cast<size_t>(luaL_checkinteger(L, 2));
    if (0 == n) {
        return moon::lua_argferror(
            L,
            2,
            "buffer.prepare: size must be greater than 0, got %I",
            (lua_Integer)n
        );
    }
    buf->prepare(n);
    return 0;
}

static int unsafe_delete(lua_State* L) {
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    auto* buf = static_cast<buffer*>(lua_touserdata(L, 1));
    luaL_argcheck(L, buf != nullptr, 1, "expected valid buffer lightuserdata");
    delete buf;
    return 0;
}

static int unsafe_new(lua_State* L) {
    lua_Integer capacity_arg = luaL_optinteger(L, 1, buffer::DEFAULT_CAPACITY);

    // Check for valid capacity
    if (capacity_arg <= 0) {
        return moon::lua_argferror(
            L,
            1,
            "buffer.unsafe_new: capacity must be positive, got %I",
            capacity_arg
        );
    }

    // Check for reasonable upper limit to prevent excessive memory allocation
    constexpr lua_Integer MAX_CAPACITY = 1024 * 1024 * 1024; // 1GB
    if (capacity_arg > MAX_CAPACITY) {
        return moon::lua_argferror(
            L,
            1,
            "buffer.unsafe_new: capacity too large (max %I), got %I",
            MAX_CAPACITY,
            capacity_arg
        );
    }

    auto capacity = static_cast<size_t>(capacity_arg);
    auto* buf = new buffer { capacity };
    lua_pushlightuserdata(L, buf);
    return 1;
}

static int concat(lua_State* L) {
    int n = lua_gettop(L);
    if (0 == n) {
        return 0;
    }
    try {
        auto buf = std::make_unique<moon::buffer>(256);
        buf->commit_unchecked(BUFFER_OPTION_CHEAP_PREPEND);
        for (int i = 1; i <= n;) {
            i += write_one(L, buf.get(), i, 0);
        }
        buf->consume_unchecked(BUFFER_OPTION_CHEAP_PREPEND);
        lua_pushlightuserdata(L, buf.release());
        return 1;
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

static int concat_string(lua_State* L) {
    int n = lua_gettop(L);
    if (0 == n) {
        lua_pushliteral(L, "");
        return 1;
    }

    try {
        buffer buf{static_cast<size_t>(n * 16)};
        for (int i = 1; i <= n;) {
            i += write_one(L, &buf, i, 0);
        }
        lua_pushlstring(L, buf.data(), buf.size());
        return 1;
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

static int to_shared(lua_State* L) {
    if (lua_type(L, 1) != LUA_TLIGHTUSERDATA) {
        return moon::lua_argferror(
            L,
            1,
            "buffer.to_shared: expected buffer lightuserdata, got %s",
            lua_typename(L, lua_type(L, 1))
        );
    }

    auto* b = static_cast<moon::buffer*>(lua_touserdata(L, 1));
    if (nullptr == b) {
        return luaL_argerror(
            L,
            1,
            "buffer.to_shared: expected buffer lightuserdata, got null pointer"
        );
    }

    if (b->size() == 0) {
        return 0;
    }

    void* space = lua_newuserdatauv(L, sizeof(moon::buffer_shr_ptr_t), 0);
    new (space) moon::buffer_shr_ptr_t { b };
    if (luaL_newmetatable(L, "lbuffer_shr_ptr") != 0) //mt
    {
        auto gc = [](lua_State* L) {
            auto* shr = static_cast<moon::buffer_shr_ptr_t*>(lua_touserdata(L, 1));
            if (nullptr == shr) {
                return luaL_argerror(L, 1, "buffer.__gc: invalid buffer_shr_ptr_t pointer");
            }
            std::destroy_at(shr);
            return 0;
        };
        lua_pushcclosure(L, gc, 0);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
    return 1;
}

static int has_bitmask(lua_State* L) {
    auto* buf = get_pointer(L, 1);
    bool has = buf->has_bitmask(static_cast<socket_send_mask>(luaL_checkinteger(L, 2)));
    lua_pushboolean(L, has ? 1 : 0);
    return 1;
}

static int add_bitmask(lua_State* L) {
    auto* buf = get_pointer(L, 1);
    auto bitmask = static_cast<socket_send_mask>(luaL_checkinteger(L, 2));
    buf->add_bitmask(bitmask);
    return 0;
}

static int append(lua_State* L) {
    auto* buf = get_pointer(L, 1);
    try {
        if (int n = lua_gettop(L); n == 2) {
            auto* append = get_pointer(L, 2);
            buf->write_back({ append->data(), append->size() });
        } else {
            std::array<const buffer*, 64> bufs;
            int count = n - 1;
            if(bufs.size() < static_cast<size_t>(count)) {
                return moon::lua_argferror(
                    L,
                    1,
                    "buffer.append: too many buffers to append, max supported is %I, got %I",
                    (lua_Integer)bufs.size(),
                    (lua_Integer)count
                );
            }

            size_t total = 0;
            for (int i = 0; i < count; i++) {
                bufs[i] = get_pointer(L, i + 2);
                total += bufs[i]->size();
            }
            auto [ptr, _] = buf->prepare(total);
            size_t offset = 0;
            for (int i = 0; i < count; i++) {
                memcpy(ptr + offset, bufs[i]->data(), bufs[i]->size());
                offset += bufs[i]->size();
            }
            buf->commit_unchecked(offset);
        }
        return 0;
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

LUAMOD_API int luaopen_buffer(lua_State* L) {
    luaL_Reg l[] = {
        { "unsafe_new", unsafe_new },
        { "pack", pack },
        { "concat", concat },
        { "delete", unsafe_delete },
        { "to_shared", to_shared },
        { "clear", clear },
        { "size", size },
        { "unpack", unpack },
        { "read", read },
        { "write_front", write_front },
        { "write_back", write_back },
        { "seek", seek },
        { "commit", commit },
        { "prepare", prepare },
        { "concat_string", concat_string },
        { "has_bitmask", has_bitmask },
        { "add_bitmask", add_bitmask },
        { "append", append },
        { nullptr, nullptr },
    };
    luaL_newlib(L, l);
    return 1;
}
