#include "lua.hpp"
#include "common/time.hpp"
#include "common/timer.hpp"
#include "common/md5.hpp"
#include "common/byte_convert.hpp"
#include "common/lua_utility.hpp"
#include "message.hpp"
#include "server.h"
#include "worker.h"
#include "services/lua_service.h"

using namespace moon;

static moon::buffer_ptr_t moon_to_buffer(lua_State* L, int index)
{
    int t = lua_type(L, index);
    switch (t)
    {
    case LUA_TNIL:
    {
        return nullptr;
    }
    case LUA_TSTRING:
    {
        std::size_t len;
        auto str = lua_tolstring(L, index, &len);
        auto buf = moon::message::create_buffer(len);
        buf->write_back(str, len);
        return buf;
    }
    case LUA_TLIGHTUSERDATA:
    {
        moon::buffer* p = static_cast<moon::buffer*>(lua_touserdata(L, index));
        return moon::buffer_ptr_t(p);
    }
    default:
        luaL_argerror(L, index, "nil, lightuserdata(buffer*) or string expected");
    }
    return nullptr;
}

static int lmoon_clock(lua_State* L)
{
    lua_pushnumber(L, time::clock());
    return 1;
}

static int lmoon_md5(lua_State* L)
{
    size_t size;
    const char* s = luaL_checklstring(L, 1, &size);
    uint8_t buf[md5::DIGEST_BYTES] = { 0 };
    md5::md5_context ctx;
    md5::init(ctx);
    md5::update(ctx, s, size);
    md5::finish(ctx, buf);

    char res[md5::DIGEST_BYTES * 2];
    for (size_t i = 0; i < 16; i++) {
        int t = buf[i];
        int a = t / 16;
        int b = t % 16;
        res[i * 2] = md5::HEX[a];
        res[i * 2 + 1] = md5::HEX[b];
    }
    lua_pushlstring(L, res, sizeof(res));
    return 1;
}

static int lmoon_tostring(lua_State* L)
{
    const char* data = (const char*)lua_touserdata(L, 1);
    luaL_argcheck(L, nullptr != data, 1, "lightuserdata(char*) expected");
    size_t len = luaL_checkinteger(L, 2);
    lua_pushlstring(L, data, len);
    return 1;
}

static int lmoon_timeout(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    int64_t interval = luaL_checkinteger(L, 1);
    uint32_t timerid = S->get_server()->timeout(interval, S->id());
    lua_pushinteger(L, timerid);
    return 1;
}

static int lmoon_log(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    auto level = (moon::LogLevel)luaL_checkinteger(L, 1);
    if (S->logger()->get_level() < level)
        return 0;

    moon::buffer line = S->logger()->make_line(true, level, S->id());

    int n = lua_gettop(L);  /* number of arguments */
    int i;
    for (i = 2; i <= n; i++) {  /* for each argument */
        size_t l;
        const char* s = luaL_tolstring(L, i, &l);  /* convert it to string */
        if (i > 2)  /* not the first element? */
            line.write_back('\t');  /* add a tab before it */
        line.write_back(s, l);  /* print it */
        lua_pop(L, 1);  /* pop result */
    }

    lua_Debug ar;
    if (lua_getstack(L, 2, &ar))
    {
        if (lua_getinfo(L, "Sl", &ar))
        {
            line.write_back('\t');
            line.write_back('(');
            if (ar.srclen>1)
                line.write_back(ar.source + 1, ar.srclen - 1);
            line.write_back(':');
            line.write_chars(ar.currentline);
            line.write_back(')');
        }
    }
    S->logger()->push_line(std::move(line));
    return 0;
}

static int lmoon_loglevel(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    if(lua_type(L, 1) == LUA_TSTRING)
        S->logger()->set_level(moon::lua_check<std::string_view>(L, 1));
    lua_pushinteger(L, (lua_Integer)S->logger()->get_level());
    return 1;
}

static int lmoon_cpu(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    lua_pushinteger(L, (lua_Integer)S->cpu_cost());
    return 1;
}

static int lmoon_send(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    uint32_t receiver = (uint32_t)luaL_checkinteger(L, 1);
    luaL_argcheck(L, receiver > 0, 1, "receiver must > 0");
    int32_t sessionid = (int32_t)luaL_checkinteger(L, 3);
    uint8_t type = (uint8_t)luaL_checkinteger(L, 4);
    luaL_argcheck(L, type > 0, 4, "PTYPE must > 0");

    S->get_server()->send(S->id(), receiver, moon_to_buffer(L, 2), sessionid, type);
    return 0;
}

static void table_tostring(std::string& res, lua_State* L, int index)
{
    if (index < 0) {
        index = lua_gettop(L) + index + 1;
    }

    luaL_checkstack(L, LUA_MINSTACK, NULL);
    res.append("{");
    lua_pushnil(L);
    while (lua_next(L, index))
    {
        res.append(lua_tostring(L, -2));
        res.append("=");
        switch (lua_type(L, -1))
        {
        case LUA_TNUMBER:
        {
            if (lua_isinteger(L, -1))
                res.append(std::to_string(lua_tointeger(L, -1)));
            else
                res.append(std::to_string(lua_tonumber(L, -1)));
            break;
        }
        case LUA_TBOOLEAN:
        {
            res.append(lua_toboolean(L, -1) ? "true" : "false");
            break;
        }
        case LUA_TSTRING:
        {
            res.append("'");
            res.append(lua_check<std::string_view>(L, -1));
            res.append("'");
            break;
        }
        case LUA_TTABLE:
        {
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

static int lmoon_new_service(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TTABLE);

    lua_service* S = lua_service::get(L);
    std::unique_ptr<moon::service_conf> conf = std::make_unique<moon::service_conf>();

    conf->creator = S->id();
    conf->session = (int32_t)luaL_checkinteger(L, 1);
    conf->name = lua_opt_field<std::string>(L, 2, "name");
    conf->type = lua_opt_field<std::string>(L, 2, "stype", "lua");
    conf->source = lua_opt_field<std::string>(L, 2, "file");
    conf->memlimit = lua_opt_field<size_t>(L, 2, "memlimit",  std::numeric_limits<size_t>::max());
    conf->unique = lua_opt_field<bool>(L, 2, "unique", false);
    conf->threadid = lua_opt_field<uint32_t>(L, 2, "threadid", 0);

    auto path = S->get_server()->get_env("PATH");
    if(path)
        conf->params.append(*path);
    conf->params.append("return ");
    table_tostring(conf->params, L, 2);

    S->get_server()->new_service(std::move(conf));
    return 0;
}

static int lmoon_kill(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    uint32_t serviceid = (uint32_t)luaL_checkinteger(L, 1);
    int32_t sessionid = (int32_t)luaL_optinteger(L, 2, 0);
    if (S->id() == serviceid)
        S->ok(false);
    S->get_server()->remove_service(serviceid, S->id(), sessionid);
    return 0;
}

static int lmoon_scan_services(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    uint32_t workerid = (uint32_t)luaL_checkinteger(L, 1);
    int32_t sessionid = (int32_t)luaL_checkinteger(L, 2);
    S->get_server()->scan_services(S->id(), workerid, sessionid);
    return 0;
}

static int lmoon_queryservice(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    std::string_view name = lua_check<std::string_view>(L, 1);
    uint32_t id = S->get_server()->get_unique_service(std::string{ name });
    lua_pushinteger(L, id);
    return 1;
}

static int lmoon_env(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    if(lua_gettop(L) == 2)
    {
        S->get_server()->set_env(lua_check<std::string>(L, 1), lua_check<std::string>(L, 2));
        return 0;
    }
    else
    {
        auto value = S->get_server()->get_env(lua_check<std::string>(L, 1));
        if (!value)
            return 0;
        lua_pushlstring(L, value->data(), value->size());
        return 1;
    }
}

static int lmoon_server_stats(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    std::string_view opt = luaL_optstring(L, 1, "");
    if(opt == "service.count")
        lua_pushinteger(L, S->get_server()->service_count());
    else if(opt == "log.error")
        lua_pushinteger(L, S->logger()->error_count());
    else{
        std::string info = S->get_server()->info();
        lua_pushlstring(L, info.data(), info.size());
    }
    return 1;
}

static int lmoon_exit(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    int32_t code = (int32_t)luaL_checkinteger(L, 1);
    S->get_server()->stop(code);
    return 0;
}

static int lmoon_now(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    auto ratio = luaL_optinteger(L, 1, 1);
    luaL_argcheck(L, ratio > 0, 1, "must >0");
    time_t t = (S->get_server()->now()/ratio);
    lua_pushinteger(L, t);
    return 1;
}

static int lmoon_adjtime(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    time_t t = luaL_checkinteger(L, 1);
    bool ok = time::offset(t);
    S->get_server()->now(true);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int message_decode(lua_State* L)
{
    message* m = (message*)lua_touserdata(L, 1);
    if (nullptr == m)
        return luaL_argerror(L, 1, "lightuserdata(message*) expected");
    size_t len = 0;
    const char* sz = luaL_checklstring(L, 2, &len);
    int top = lua_gettop(L);
    for (size_t i = 0; i < len; ++i)
    {
        switch (sz[i])
        {
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
            if(m->data()!=nullptr)
                lua_pushlstring(L, m->data(), m->size());
            else
                lua_pushnil(L);
            break;
        case 'N':
        {
            lua_pushinteger(L, m->size());
            break;
        }
        case 'B':
        {
            lua_pushlightuserdata(L, m->get_buffer());
            break;
        }
        case 'C':
        {
            buffer* buf = m->get_buffer();
            if (nullptr == buf)
            {
                lua_pushlightuserdata(L, nullptr);
                lua_pushinteger(L, 0);
            }
            else
            {
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

static int message_clone(lua_State* L)
{
    message* m = (message*)lua_touserdata(L, 1);
    if (nullptr == m)
        return luaL_argerror(L, 1, "lightuserdata(message*) expected");
    message* nm = new message((const buffer_ptr_t&)*m);
    nm->set_broadcast(m->broadcast());
    nm->set_receiver(m->receiver());
    nm->set_sender(m->sender());
    nm->set_sessionid(m->sessionid());
    nm->set_type(m->type());
    lua_pushlightuserdata(L, nm);
    return 1;
}

static int message_release(lua_State* L)
{
    message* m = (message*)lua_touserdata(L, 1);
    if (nullptr == m)
        return luaL_argerror(L, 1, "lightuserdata(message*) expected");
    delete m;
    return 0;
}

static int message_redirect(lua_State* L)
{
    int top = lua_gettop(L);
    message* m = (message*)lua_touserdata(L, 1);
    if (nullptr == m)
        return luaL_argerror(L, 1, "lightuserdata(message*) expected");
    m->set_receiver((uint32_t)luaL_checkinteger(L, 2));
    m->set_type((uint8_t)luaL_checkinteger(L, 3));
    if (top > 3)
    {
        m->set_sender((uint32_t)luaL_checkinteger(L, 4));
        m->set_sessionid((int32_t)luaL_checkinteger(L, 5));
    }
    return 0;
}

static int lmi_collect(lua_State* L)
{
    (void)L;
#ifdef MOON_ENABLE_MIMALLOC
    bool force = (luaL_opt(L, lua_toboolean, 2, 1)!=0);
    mi_collect(force);
#endif
    return 0;
}

extern "C" {
    int LUAMOD_API luaopen_moon(lua_State* L)
    {
    luaL_Reg l[] = {
        { "clock", lmoon_clock},
        { "md5", lmoon_md5 },
        { "tostring", lmoon_tostring },
        { "timeout", lmoon_timeout},
        { "log", lmoon_log},
        { "loglevel", lmoon_loglevel},
        { "cpu", lmoon_cpu},
        { "send", lmoon_send},
        { "new_service", lmoon_new_service},
        { "kill", lmoon_kill},
        { "scan_services", lmoon_scan_services},
        { "queryservice", lmoon_queryservice},
        { "env", lmoon_env},
        { "server_stats", lmoon_server_stats},
        { "exit", lmoon_exit},
        { "now", lmoon_now},
        { "adjtime", lmoon_adjtime},
        { "callback", lua_service::set_callback},
        { "decode", message_decode},
        { "clone", message_clone },
        { "release", message_release },
        { "redirect", message_redirect},
        { "collect", lmi_collect},
        /* placeholders */
        { "id", NULL},
        { "name", NULL}, 
        { "timezone", NULL},
        {NULL,NULL}
    };

    luaL_newlib(L, l);
    lua_service* S = lua_service::get(L);
    lua_pushinteger(L, S->id());
    lua_setfield(L, -2, "id");
    lua_pushlstring(L, S->name().data(), S->name().size());
    lua_setfield(L, -2, "name");
    lua_pushinteger(L, moon::time::timezone());
    lua_setfield(L, -2, "timezone");
    return 1;
    }
}

static int lasio_try_open(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket();
    std::string_view host = lua_check<std::string_view>(L, 1);
    uint16_t port = (uint16_t)luaL_checkinteger(L, 2);
    bool ok = sock.try_open(std::string{host}, port);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_listen(lua_State* L)
{
    lua_service* S  = lua_service::get(L);
    auto& sock = S->get_worker()->socket();
    std::string_view host = lua_check<std::string_view>(L, 1);
    uint16_t port = (uint16_t)luaL_checkinteger(L, 2);
    uint8_t type = (uint8_t)luaL_checkinteger(L, 3);
    auto res  = sock.listen(std::string{ host }, port, S->id(), type);
    lua_pushinteger(L, res.first);
    if (res.first > 0)
    {
        lua_pushstring(L, res.second.address().to_string().data());
        lua_pushinteger(L, res.second.port());
        return 3;
    }
    return 1;
}

static int lasio_accept(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket();
    uint32_t fd = (uint32_t)luaL_checkinteger(L, 1);
    int32_t sessionid = (int32_t)luaL_checkinteger(L, 2);
    uint32_t owner = (uint32_t)luaL_checkinteger(L, 3);
    bool ok = sock.accept(fd, sessionid, owner);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_connect(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket();
    std::string host = lua_check<std::string>(L, 1);
    uint16_t port = (uint16_t)luaL_checkinteger(L, 2);
    uint8_t type = (uint8_t)luaL_checkinteger(L, 3);
    int32_t sessionid = (int32_t)luaL_checkinteger(L, 4);
    uint32_t timeout = (uint32_t)luaL_checkinteger(L, 5);
    std::string payload = luaL_optstring(L, 6, "");
    uint32_t fd = sock.connect(host, port, S->id(), type, sessionid, timeout, std::move(payload));
    lua_pushinteger(L, fd);
    return 1;
}

static int lasio_read(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket();
    uint32_t fd = (uint32_t)luaL_checkinteger(L, 1);
    int32_t sessionid = (int32_t)luaL_checkinteger(L, 2);
    int64_t size = 0;
    std::string_view delim;
    if (lua_type(L, 3) == LUA_TNUMBER)
    {
        size = (int64_t)luaL_checkinteger(L, 3);
    }
    else
    {
        delim = lua_check<std::string_view>(L, 3);
        size = (int64_t)luaL_optinteger(L, 4, 0);
    }
    sock.read(fd, S->id(), size, delim, sessionid);
    return 0;
}

static int lasio_write(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket();
    uint32_t fd = (uint32_t)luaL_checkinteger(L, 1);
    int flag = (int)luaL_optinteger(L, 3, 0);
    if (flag!=0 && (flag<=0 || flag >=(int)moon::buffer_flag::buffer_flag_max))
    {
        return luaL_error(L, "asio.write param 'flag' invalid");
    }
    bool ok = sock.write(fd, moon_to_buffer(L, 2), (moon::buffer_flag)flag);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_write_message(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket();
    uint32_t fd = (uint32_t)luaL_checkinteger(L, 1);
    moon::message* m =(moon::message*)lua_touserdata(L, 2);
    if (nullptr == m)
        return luaL_argerror(L, 2, "lightuserdata(message*) expected");
    bool ok = sock.write(fd, *m);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_close(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket();
    uint32_t fd = (uint32_t)luaL_checkinteger(L, 1);
    bool ok = sock.close(fd);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_settimeout(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket();
    uint32_t fd = (uint32_t)luaL_checkinteger(L, 1);
    uint32_t seconds = (uint32_t)luaL_checkinteger(L, 2);
    bool ok = sock.settimeout(fd, seconds);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_setnodelay(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket();
    uint32_t fd = (uint32_t)luaL_checkinteger(L, 1);
    bool ok = sock.setnodelay(fd);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_set_enable_chunked(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket();
    uint32_t fd = (uint32_t)luaL_checkinteger(L, 1);
    std::string_view flag = lua_check<std::string_view>(L, 2);
    bool ok = sock.set_enable_chunked(fd, flag);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_set_send_queue_limit(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket();
    uint32_t fd = (uint32_t)luaL_checkinteger(L, 1);
    uint32_t warnsize = (uint32_t)luaL_checkinteger(L, 2);
    uint32_t errorsize = (uint32_t)luaL_checkinteger(L, 3);
    bool ok = sock.set_send_queue_limit(fd, warnsize, errorsize);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_address(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket();
    uint32_t fd = (uint32_t)luaL_checkinteger(L, 1);
    std::string addr = sock.getaddress(fd);
    lua_pushlstring(L, addr.data(), addr.size());
    return 1;
}

static int lasio_udp(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket();
    size_t size = 0;
    const char* addr = lua_tolstring(L, 1, &size);
    asio::ip::port_type port = 0;
    std::string_view address;
    if (addr)
    {
        address = std::string_view{ addr, size };
        port = (asio::ip::port_type)luaL_checkinteger(L, 2);
    }
    uint32_t fd = sock.udp(S->id(), address, port);
    lua_pushinteger(L, fd);
    return 1;
}

static int lasio_udp_connect(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket();
    uint32_t fd = (uint32_t)luaL_checkinteger(L, 1);
    auto ok = sock.udp_connect(fd, lua_check<std::string_view>(L, 2), (asio::ip::port_type)luaL_checkinteger(L, 3));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_sendto(lua_State* L)
{
    lua_service* S = lua_service::get(L);
    auto& sock = S->get_worker()->socket();
    uint32_t fd = (uint32_t)luaL_checkinteger(L, 1);
    std::string_view address = lua_check<std::string_view>(L, 2);
    bool ok = sock.send_to(fd, address, moon_to_buffer(L, 3));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lasio_make_endpoint(lua_State* L)
{
    size_t size = 0;
    const char* str = luaL_checklstring(L, 1, &size);
    auto port = (asio::ip::port_type)luaL_checkinteger(L, 2);
    if (size == 0 || nullptr == str)
        return 0;
    asio::error_code ec;
    auto addr = asio::ip::make_address(std::string_view{ str, size }, ec);
    if (ec)
    {
        auto message = ec.message();
        lua_pushboolean(L, 0);
        lua_pushlstring(L, message.data(), message.size());
        return 2;
    }
    char arr[32];
    size = moon::socket::encode_endpoint(arr, addr, port);
    lua_pushlstring(L, arr, size);
    return 1;
}

static int lasio_unpack_udp(lua_State* L)
{
    size_t size = 0;
    const char* str = nullptr;
    if (lua_type(L, 1) == LUA_TSTRING) {
        str = luaL_checklstring(L, 1, &size);
    }
    else {
        str = reinterpret_cast<const char*>(lua_touserdata(L, 1));
        size = luaL_checkinteger(L, 2);
    }
    if (size == 0 || nullptr == str)
        return 0;
    size_t addr_size = ((str[0] == '4') ? moon::socket::addr_v4_size : moon::socket::addr_v6_size);
    lua_pushlstring(L, str, addr_size);
    str += addr_size;
    lua_pushlstring(L, str, size - addr_size);
    return 2;
}

extern "C" {
    int LUAMOD_API luaopen_asio(lua_State* L)
    {
    luaL_Reg l[] = {
        { "try_open", lasio_try_open},
        { "listen", lasio_listen },
        { "accept", lasio_accept },
        { "connect", lasio_connect },
        { "read", lasio_read},
        { "write", lasio_write},
        { "write_message", lasio_write_message},
        { "close", lasio_close},
        { "settimeout", lasio_settimeout},
        { "setnodelay", lasio_setnodelay},
        { "set_enable_chunked", lasio_set_enable_chunked},
        { "set_send_queue_limit", lasio_set_send_queue_limit},
        { "getaddress", lasio_address},
        { "udp", lasio_udp},
        { "udp_connect", lasio_udp_connect},
        { "sendto", lasio_sendto},
        { "make_endpoint", lasio_make_endpoint},
        { "unpack_udp", lasio_unpack_udp},
        {NULL,NULL}
    };
    luaL_newlib(L, l);
    return 1;
    }
}
