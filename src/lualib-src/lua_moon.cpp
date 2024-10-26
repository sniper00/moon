#include "common/byte_convert.hpp"
#include "common/lua_utility.hpp"
#include "common/md5.hpp"
#include "common/time.hpp"
#include "common/timer.hpp"
#include "lua.hpp"
#include "message.hpp"
#include "server.h"
#include "services/lua_service.h"
#include "worker.h"

using namespace moon;

static moon::buffer_ptr_t moon_to_buffer(lua_State* L, int index) {
    switch (lua_type(L, index)) {
        case LUA_TNIL:
        case LUA_TNONE: {
            return nullptr;
        }
        case LUA_TSTRING: {
            std::size_t len;
            auto str = lua_tolstring(L, index, &len);
            auto buf = moon::buffer::make_unique(len);
            buf->write_back(str, len);
            return buf;
        }
        case LUA_TLIGHTUSERDATA: {
            return moon::buffer_ptr_t { static_cast<moon::buffer*>(lua_touserdata(L, index)) };
        }
        default:
            luaL_argerror(L, index, "nil, lightuserdata(buffer*) or string expected");
    }
    return nullptr;
}

static moon::buffer_shr_ptr_t moon_to_shr_buffer(lua_State* L, int index) {
    switch (lua_type(L, index)) {
        case LUA_TNIL: {
            return nullptr;
        }
        case LUA_TSTRING: {
            std::size_t len;
            auto str = lua_tolstring(L, index, &len);
            auto buf = buffer::make_shared(len);
            buf->write_back(str, len);
            return buf;
        }
        case LUA_TLIGHTUSERDATA: {
            return moon::buffer_shr_ptr_t { static_cast<moon::buffer*>(lua_touserdata(L, index)) };
        }
        default:
            luaL_argerror(L, index, "nil, lightuserdata(buffer*) or string expected");
    }
    return nullptr;
}

static int lmoon_clock(lua_State* L) {
    lua_pushnumber(L, time::clock());
    return 1;
}

static int lmoon_md5(lua_State* L) {
    size_t size;
    const char* s = luaL_checklstring(L, 1, &size);
    std::array<uint8_t, md5::DIGEST_BYTES> bytes {};
    md5::md5_context ctx;
    md5::init(ctx);
    md5::update(ctx, s, size);
    md5::finish(ctx, bytes.data());

    std::array<char, md5::DIGEST_BYTES * 2> str {};
    for (size_t i = 0; i < 16; i++) {
        int t = bytes[i];
        int a = t / 16;
        int b = t % 16;
        str[i * 2] = md5::HEX[a];
        str[i * 2 + 1] = md5::HEX[b];
    }
    lua_pushlstring(L, str.data(), str.size());
    return 1;
}

static int lmoon_tostring(lua_State* L) {
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    auto* data = (const char*)lua_touserdata(L, 1);
    size_t len = luaL_checkinteger(L, 2);
    lua_pushlstring(L, data, len);
    return 1;
}

static int lmoon_timeout(lua_State* L) {
    lua_service* S = lua_service::get(L);
    int64_t interval = luaL_checkinteger(L, 1);
    int64_t timer_id = S->next_sequence();
    S->get_server()->timeout(interval, S->id(), -timer_id);
    lua_pushinteger(L, timer_id);
    return 1;
}

static int lmoon_log(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    auto level = (moon::LogLevel)luaL_checkinteger(L, 1);
    if (log::instance().get_level() < level)
        return 0;

    moon::buffer line = log::instance().make_line(true, level, S->id());

    int n = lua_gettop(L); /* number of arguments */
    for (int i = 2; i <= n; i++) { /* for each argument */
        if (i > 2) /* not the first element? */
            line.write_back("    ", 4); /* add a tab before it */
        switch (lua_type(L, i)) {
            case LUA_TNIL:
                line.write_back("nil", 3);
                break;
            case LUA_TNUMBER: {
                if (lua_isinteger(L, i))
                    line.write_chars(lua_tointeger(L, i));
                else
                    line.write_chars(lua_tonumber(L, i));
                break;
            }
            case LUA_TBOOLEAN: {
                int v = lua_toboolean(L, i);
                constexpr std::string_view s[2] = { "true", "false" };
                line.write_back(s[v].data(), s[v].size());
                break;
            }
            case LUA_TSTRING: {
                size_t sz = 0;
                const char* str = lua_tolstring(L, i, &sz);
                line.write_back(str, sz);
                break;
            }
            default:
                size_t l;
                const char* s = luaL_tolstring(L, i, &l); /* convert it to string */
                line.write_back(s, l); /* print it */
                lua_pop(L, 1); /* pop result */
        }
    }

    if (lua_Debug ar; lua_getstack(L, 2, &ar) && lua_getinfo(L, "Sl", &ar)) {
        line.write_back("    (", 5);
        if (ar.srclen > 1)
            line.write_back(ar.source + 1, ar.srclen - 1);
        line.write_back(':');
        line.write_chars(ar.currentline);
        line.write_back(')');
    }
    log::instance().push_line(std::move(line));
    return 0;
}

static int lmoon_loglevel(lua_State* L) {
    if (lua_type(L, 1) == LUA_TSTRING)
        log::instance().set_level(moon::lua_check<std::string_view>(L, 1));
    lua_pushinteger(L, (lua_Integer)log::instance().get_level());
    return 1;
}

static int lmoon_cpu(lua_State* L) {
    lua_service* S = lua_service::get(L);
    lua_pushinteger(L, (lua_Integer)S->cpu());
    return 1;
}

static int lmoon_send(lua_State* L) {
    lua_service* S = lua_service::get(L);

    auto type = (uint8_t)luaL_checkinteger(L, 1);
    luaL_argcheck(L, type > 0, 1, "PTYPE must > 0");

    auto receiver = (uint32_t)luaL_checkinteger(L, 2);
    luaL_argcheck(L, receiver > 0, 2, "receiver must > 0");

    int64_t session = luaL_opt(L, luaL_checkinteger, 4, S->next_sequence());

    S->get_server()->send(S->id(), receiver, moon_to_buffer(L, 3), session, type);

    lua_pushinteger(L, session);
    lua_pushinteger(L, receiver);
    return 2;
}

static void table_tostring(std::string& res, lua_State* L, int index) {
    if (index < 0) {
        index = lua_gettop(L) + index + 1;
    }

    luaL_checkstack(L, LUA_MINSTACK, nullptr);
    res.append("{");
    lua_pushnil(L);
    while (lua_next(L, index)) {
        res.append(lua_tostring_unchecked(L, -2));
        res.append("=");
        switch (lua_type(L, -1)) {
            case LUA_TNUMBER: {
                if (lua_isinteger(L, -1))
                    res.append(std::to_string(lua_tointeger(L, -1)));
                else
                    res.append(std::to_string(lua_tonumber(L, -1)));
                break;
            }
            case LUA_TBOOLEAN: {
                res.append(lua_toboolean(L, -1) ? "true" : "false");
                break;
            }
            case LUA_TSTRING: {
                res.append("'");
                res.append(lua_check<std::string_view>(L, -1));
                res.append("'");
                break;
            }
            case LUA_TTABLE: {
                table_tostring(res, L, -1);
                break;
            }
            default:
                res.append("false");
                break;
        }
        res.append(",");
        lua_pop(L, 1);
    }
    res.append("}");
}

static std::optional<std::string>
search_file(std::string_view lua_search_path, std::string_view source) {
    size_t first_quote_pos = lua_search_path.find_first_of("'");
    if (first_quote_pos != std::string_view::npos) {
        size_t second_quote_pos = lua_search_path.find_first_of("'", first_quote_pos + 1);
        if (second_quote_pos != std::string_view::npos) {
            auto extracted_path =
                lua_search_path.substr(first_quote_pos + 1, second_quote_pos - first_quote_pos - 1);
            auto path_array = moon::split<std::string>(extracted_path, ";");
            for (auto& path: path_array) {
                moon::replace(path, "?.lua", source);
                if (fs::exists(path)) {
                    return path;
                }
            }
        }
    }
    return std::nullopt;
}

static int lmoon_new_service(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_service* S = lua_service::get(L);
    auto conf = std::make_unique<moon::service_conf>();

    conf->creator = S->id();
    int64_t session = S->next_sequence();
    conf->session = session;
    conf->name = lua_opt_field<std::string>(L, 1, "name");
    conf->type = lua_opt_field<std::string>(L, 1, "stype", "lua");
    conf->source = lua_opt_field<std::string>(L, 1, "file");
    conf->memlimit = lua_opt_field<ssize_t>(L, 1, "memlimit", std::numeric_limits<ssize_t>::max());
    conf->unique = lua_opt_field<bool>(L, 1, "unique", false);
    conf->threadid = lua_opt_field<uint32_t>(L, 1, "threadid", 0);

    if (auto path = S->get_server()->get_env("PATH"); path) {
        conf->params.append(*path);
        if (!fs::exists(conf->source)) {
            if (auto maybe_path = search_file(*path, conf->source)) {
                conf->source = maybe_path.value();
            }
        }
    }

    if (auto cpath = S->get_server()->get_env("CPATH"); cpath)
        conf->params.append(*cpath);
    conf->params.append("return ");
    table_tostring(conf->params, L, 1);

    S->get_server()->new_service(std::move(conf));
    lua_pushinteger(L, session);
    return 1;
}

static int lmoon_kill(lua_State* L) {
    lua_service* S = lua_service::get(L);
    auto serviceid = (uint32_t)luaL_checkinteger(L, 1);
    if (S->id() == serviceid)
        S->ok(false);
    S->get_server()->remove_service(serviceid, S->id(), 0);
    return 0;
}

static int lmoon_scan_services(lua_State* L) {
    lua_service* S = lua_service::get(L);
    auto workerid = (uint32_t)luaL_checkinteger(L, 1);
    int64_t sessionid = S->next_sequence();
    S->get_server()->scan_services(S->id(), workerid, sessionid);
    lua_pushinteger(L, sessionid);
    return 1;
}

static int lmoon_queryservice(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    std::string_view name = lua_check<std::string_view>(L, 1);
    uint32_t id = S->get_server()->get_unique_service(std::string { name });
    lua_pushinteger(L, id);
    return 1;
}

static int lmoon_next_sequence(lua_State* L) {
    lua_service* S = lua_service::get(L);
    lua_pushinteger(L, S->next_sequence());
    return 1;
}

static int lmoon_env(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    if (lua_gettop(L) == 2) {
        S->get_server()->set_env(lua_check<std::string>(L, 1), lua_check<std::string>(L, 2));
        return 0;
    } else {
        auto value = S->get_server()->get_env(lua_check<std::string>(L, 1));
        if (!value)
            return 0;
        lua_pushlstring(L, value->data(), value->size());
        return 1;
    }
}

static int lmoon_server_stats(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    if (std::string_view opt = luaL_optstring(L, 1, ""); opt == "service.count")
        lua_pushinteger(L, S->get_server()->service_count());
    else if (opt == "log.error")
        lua_pushinteger(L, log::instance().error_count());
    else {
        std::string info = S->get_server()->info();
        lua_pushlstring(L, info.data(), info.size());
    }
    return 1;
}

static int lmoon_exit(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    auto code = (int32_t)luaL_checkinteger(L, 1);
    S->get_server()->stop(code);
    return 0;
}

static int lmoon_now(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    auto ratio = luaL_optinteger(L, 1, 1);
    luaL_argcheck(L, ratio > 0, 1, "must >0");
    time_t t = (S->get_server()->now() / ratio);
    lua_pushinteger(L, t);
    return 1;
}

static int lmoon_adjtime(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    time_t t = luaL_checkinteger(L, 1);
    bool ok = time::offset(t);
    S->get_server()->now(true);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int message_decode(lua_State* L) {
    auto* m = (message*)lua_touserdata(L, 1);
    if (nullptr == m)
        return luaL_argerror(L, 1, "lightuserdata(message*) expected");
    size_t len = 0;
    const char* sz = luaL_checklstring(L, 2, &len);
    int top = lua_gettop(L);
    for (size_t i = 0; i < len; ++i) {
        switch (sz[i]) {
            case 'S':
                lua_pushinteger(L, m->sender());
                break;
            case 'R':
                lua_pushinteger(L, m->receiver());
                break;
            case 'E':
                lua_pushinteger(L, m->sessionid());
                break;
            case 'Z':
                if (m->data() != nullptr)
                    lua_pushlstring(L, m->data(), m->size());
                else
                    lua_pushnil(L);
                break;
            case 'N': {
                lua_pushinteger(L, m->size());
                break;
            }
            case 'B': {
                lua_pushlightuserdata(L, m->as_buffer());
                break;
            }
            case 'L': {
                auto buf = m->into_buffer();
                lua_pushlightuserdata(L, buf.release());
                break;
            }
            case 'C': {
                if (buffer* buf = m->as_buffer(); nullptr == buf) {
                    lua_pushlightuserdata(L, nullptr);
                    lua_pushinteger(L, 0);
                } else {
                    lua_pushlightuserdata(L, buf->data());
                    lua_pushinteger(L, buf->size());
                }
                break;
            }
            default:
                return luaL_error(L, "invalid format option '%c'", sz[i]);
        }
    }
    return lua_gettop(L) - top;
}

static int message_redirect(lua_State* L) {
    int top = lua_gettop(L);
    auto* m = (message*)lua_touserdata(L, 1);
    if (nullptr == m)
        return luaL_argerror(L, 1, "lightuserdata(message*) expected");
    m->set_receiver((uint32_t)luaL_checkinteger(L, 2));
    m->set_type((uint8_t)luaL_checkinteger(L, 3));
    if (top > 3) {
        m->set_sender((uint32_t)luaL_checkinteger(L, 4));
        m->set_sessionid(luaL_checkinteger(L, 5));
    }
    return 0;
}

static int lmi_collect(lua_State* L) {
    (void)L;
#ifdef MOON_ENABLE_MIMALLOC
    bool force = (luaL_opt(L, lua_toboolean, 2, 1) != 0);
    mi_collect(force);
#endif
    return 0;
}

extern "C" {
int LUAMOD_API luaopen_moon_core(lua_State* L) {
    luaL_Reg l[] = { { "clock", lmoon_clock },
                     { "md5", lmoon_md5 },
                     { "tostring", lmoon_tostring },
                     { "timeout", lmoon_timeout },
                     { "log", lmoon_log },
                     { "loglevel", lmoon_loglevel },
                     { "cpu", lmoon_cpu },
                     { "send", lmoon_send },
                     { "new_service", lmoon_new_service },
                     { "kill", lmoon_kill },
                     { "scan_services", lmoon_scan_services },
                     { "queryservice", lmoon_queryservice },
                     { "next_sequence", lmoon_next_sequence },
                     { "env", lmoon_env },
                     { "server_stats", lmoon_server_stats },
                     { "exit", lmoon_exit },
                     { "now", lmoon_now },
                     { "adjtime", lmoon_adjtime },
                     { "callback", lua_service::set_callback },
                     { "decode", message_decode },
                     { "redirect", message_redirect },
                     { "collect", lmi_collect },
                     /* placeholders */
                     { "id", NULL },
                     { "name", NULL },
                     { "timezone", NULL },
                     { NULL, NULL } };

    luaL_newlib(L, l);
    const lua_service* S = lua_service::get(L);
    lua_pushinteger(L, S->id());
    lua_setfield(L, -2, "id");
    lua_pushlstring(L, S->name().data(), S->name().size());
    lua_setfield(L, -2, "name");
    lua_pushinteger(L, moon::time::timezone());
    lua_setfield(L, -2, "timezone");
    return 1;
}
}

static int lasio_try_open(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket_server();
    std::string_view host = lua_check<std::string_view>(L, 1);
    auto port = (uint16_t)luaL_checkinteger(L, 2);
    bool is_connect = luaL_optboolean(L, 3, false);
    bool ok = sock.try_open(std::string { host }, port, is_connect);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_listen(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket_server();
    std::string_view host = lua_check<std::string_view>(L, 1);
    auto port = (uint16_t)luaL_checkinteger(L, 2);
    auto type = (uint8_t)luaL_checkinteger(L, 3);
    auto [k, v] = sock.listen(std::string { host }, port, S->id(), type);
    lua_pushinteger(L, k);
    if (k > 0) {
        lua_pushstring(L, v.address().to_string().data());
        lua_pushinteger(L, v.port());
        return 3;
    }
    return 1;
}

static int lasio_accept(lua_State* L) {
    lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket_server();
    auto fd = (uint32_t)luaL_checkinteger(L, 1);
    auto owner = (uint32_t)luaL_checkinteger(L, 2);
    int64_t session = luaL_opt(L, luaL_checkinteger, 3, S->next_sequence());
    if (!sock.accept(fd, session, owner)) {
        lua_pushboolean(L, 0);
        lua_pushfstring(L, "socket.accept error: fd(%I) not open or not found.", fd);
        return 2;
    }
    lua_pushinteger(L, session);
    return 1;
}

static int lasio_connect(lua_State* L) {
    lua_service* S = lua_service::get(L);

    auto& sock = S->get_worker()->socket_server();
    std::string host = lua_check<std::string>(L, 1);
    auto port = (uint16_t)luaL_checkinteger(L, 2);
    auto type = (uint8_t)luaL_checkinteger(L, 3);
    auto timeout = (uint32_t)luaL_checkinteger(L, 4);
    int64_t session = S->next_sequence();
    sock.connect(host, port, S->id(), type, session, timeout);
    lua_pushinteger(L, session);
    return 1;
}

static int lasio_read(lua_State* L) {
    lua_service* S = lua_service::get(L);
    int64_t session = S->next_sequence();
    auto& sock = S->get_worker()->socket_server();
    auto fd = (uint32_t)luaL_checkinteger(L, 1);
    int64_t size = 0;
    std::string_view delim;
    if (lua_type(L, 2) == LUA_TNUMBER) {
        size = luaL_checkinteger(L, 2);
    } else {
        delim = lua_check<std::string_view>(L, 2);
        size = luaL_optinteger(L, 3, 0);
    }

    auto res = sock.read(fd, size, delim, session);
    res.success ? lua_pushinteger(L, session) : lua_pushboolean(L, 0);

    if (res.data != nullptr) {
        lua_pushlstring(L, res.data, res.size);
        return 2;
    }

    return 1;
}

static int lasio_write(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket_server();
    auto fd = (uint32_t)luaL_checkinteger(L, 1);
    auto n = static_cast<int>(luaL_optinteger(L, 3, 0));
    luaL_argcheck(
        L,
        n >= 0 && n < static_cast<int>(moon::socket_send_mask::max_mask),
        3,
        "invalid mask"
    );
    auto mask = static_cast<moon::socket_send_mask>(n);

    if (LUA_TUSERDATA == lua_type(L, 2)) {
        auto shr = *static_cast<buffer_shr_ptr_t*>(lua_touserdata(L, 2));
        bool ok = sock.write(fd, std::move(shr), mask);
        lua_pushboolean(L, ok ? 1 : 0);
    } else {
        bool ok = sock.write(fd, moon_to_shr_buffer(L, 2), mask);
        lua_pushboolean(L, ok ? 1 : 0);
    }
    return 1;
}

static int lasio_write_message(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket_server();
    auto fd = (uint32_t)luaL_checkinteger(L, 1);
    auto* m = (message*)lua_touserdata(L, 2);
    if (nullptr == m)
        return luaL_argerror(L, 2, "lightuserdata(message*) expected");
    bool ok = sock.write(fd, m->into_buffer());
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_close(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket_server();
    auto fd = (uint32_t)luaL_checkinteger(L, 1);
    bool ok = sock.close(fd);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_switch_type(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket_server();

    auto fd = (uint32_t)luaL_checkinteger(L, 1);
    auto type = (uint8_t)luaL_checkinteger(L, 2);
    bool ok = sock.switch_type(fd, type);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_settimeout(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket_server();
    auto fd = (uint32_t)luaL_checkinteger(L, 1);
    auto seconds = (uint32_t)luaL_checkinteger(L, 2);
    bool ok = sock.settimeout(fd, seconds);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_setnodelay(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket_server();
    auto fd = (uint32_t)luaL_checkinteger(L, 1);
    bool ok = sock.setnodelay(fd);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_set_enable_chunked(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket_server();
    auto fd = (uint32_t)luaL_checkinteger(L, 1);
    std::string_view flag = lua_check<std::string_view>(L, 2);
    bool ok = sock.set_enable_chunked(fd, flag);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_set_send_queue_limit(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket_server();
    auto fd = (uint32_t)luaL_checkinteger(L, 1);
    uint16_t warnsize = moon::lua_check<uint16_t>(L, 2);
    uint16_t errorsize = moon::lua_check<uint16_t>(L, 3);
    bool ok = sock.set_send_queue_limit(fd, warnsize, errorsize);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_address(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket_server();
    auto fd = (uint32_t)luaL_checkinteger(L, 1);
    std::string addr = sock.getaddress(fd);
    lua_pushlstring(L, addr.data(), addr.size());
    return 1;
}

static int lasio_udp(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket_server();
    size_t size = 0;
    const char* addr = lua_tolstring(L, 1, &size);
    asio::ip::port_type port = 0;
    std::string_view address;
    if (addr) {
        address = std::string_view { addr, size };
        port = (asio::ip::port_type)luaL_checkinteger(L, 2);
    }
    uint32_t fd = sock.udp_open(S->id(), address, port);
    lua_pushinteger(L, fd);
    return 1;
}

static int lasio_udp_connect(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket_server();
    auto fd = (uint32_t)luaL_checkinteger(L, 1);
    auto ok = sock.udp_connect(
        fd,
        lua_check<std::string_view>(L, 2),
        (asio::ip::port_type)luaL_checkinteger(L, 3)
    );
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_sendto(lua_State* L) {
    const lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket_server();
    auto fd = (uint32_t)luaL_checkinteger(L, 1);
    std::string_view address = lua_check<std::string_view>(L, 2);
    bool ok = sock.send_to(fd, address, moon_to_shr_buffer(L, 3));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_make_endpoint(lua_State* L) {
    size_t size = 0;
    const char* str = luaL_checklstring(L, 1, &size);
    auto port = (asio::ip::port_type)luaL_checkinteger(L, 2);
    if (size == 0 || nullptr == str)
        return 0;
    asio::error_code ec;
    auto addr = asio::ip::make_address(std::string_view { str, size }, ec);
    if (ec) {
        auto message = ec.message();
        lua_pushboolean(L, 0);
        lua_pushlstring(L, message.data(), message.size());
        return 2;
    }

    auto bytes = moon::socket_server::encode_endpoint(addr, port);
    lua_pushlstring(L, bytes.data(), bytes.size());
    return 1;
}

static int lasio_unpack_udp(lua_State* L) {
    size_t size = 0;
    const char* str = nullptr;
    if (lua_type(L, 1) == LUA_TSTRING) {
        str = luaL_checklstring(L, 1, &size);
    } else {
        str = reinterpret_cast<const char*>(lua_touserdata(L, 1));
        size = luaL_checkinteger(L, 2);
    }
    if (size == 0 || nullptr == str)
        return 0;
    size_t addr_size =
        ((str[0] == '4') ? moon::socket_server::addr_v4_size : moon::socket_server::addr_v6_size);
    lua_pushlstring(L, str, addr_size);
    str += addr_size;
    lua_pushlstring(L, str, size - addr_size);
    return 2;
}

extern "C" {
int LUAMOD_API luaopen_asio_core(lua_State* L) {
    luaL_Reg l[] = { { "try_open", lasio_try_open },
                     { "listen", lasio_listen },
                     { "accept", lasio_accept },
                     { "connect", lasio_connect },
                     { "read", lasio_read },
                     { "write", lasio_write },
                     { "write_message", lasio_write_message },
                     { "close", lasio_close },
                     { "switch_type", lasio_switch_type },
                     { "settimeout", lasio_settimeout },
                     { "setnodelay", lasio_setnodelay },
                     { "set_enable_chunked", lasio_set_enable_chunked },
                     { "set_send_queue_limit", lasio_set_send_queue_limit },
                     { "getaddress", lasio_address },
                     { "udp", lasio_udp },
                     { "udp_connect", lasio_udp_connect },
                     { "sendto", lasio_sendto },
                     { "make_endpoint", lasio_make_endpoint },
                     { "unpack_udp", lasio_unpack_udp },
                     { NULL, NULL } };
    luaL_newlib(L, l);
    return 1;
}
}
