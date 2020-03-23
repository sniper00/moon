#include "asio.hpp"
#include "lua.hpp"
#include <cstdint>
#include <atomic>
#include <cassert>
#include <limits>
#include <thread>

using asio::ip::tcp;

#define METANAME "lsocket"

// Asio based block tcp client, compat luasocket interface, just for lua debug library.

struct tcp_client
{
    bool is_reading = false;
    asio::io_context io_context;
    tcp::socket socket;
    asio::streambuf buf;

    tcp_client()
        :socket(io_context)
    {

    }
};

struct tcp_box
{
    tcp_client* client = nullptr;
    size_t timeout = 0;
};

static int64_t millisecond()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static int lrelease(lua_State *L)
{
    tcp_box* box = (tcp_box*)lua_touserdata(L, 1);
    if (box)
    {
        if (box->client)
        {
            box->client->socket.close();
            delete box->client;
            box->client = nullptr;
        }
    }
    return 0;
}

static int lsettimeout(lua_State *L)
{
    tcp_box* ab = (tcp_box*)lua_touserdata(L, 1);
    if (ab == NULL)
        return luaL_error(L, "Invalid tcp_box pointer");
    double v = lua_tonumber(L, 2);//sec
    ab->timeout =static_cast<size_t>(v * 1000);//millsec
    return 0;
}

static int lconnect(lua_State *L)
{
    tcp_box* box = (tcp_box*)lua_touserdata(L, 1);
    if (box == nullptr)
        return luaL_error(L, "Invalid tcp_box pointer");
    size_t len = 0;
    const char* host = luaL_checklstring(L, 2, &len);
    int port = (int)luaL_checkinteger(L, 3);

    int top = lua_gettop(L);

    asio::error_code ec;

    auto client = new tcp_client;

    tcp::resolver resolver(client->io_context);
    asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port), ec);

    if (ec)
    {
        lua_pushnil(L);
        lua_pushlstring(L, ec.message().data(), ec.message().size());
        delete client;
        return 2;
    }

    asio::async_connect(client->socket, endpoints,
        [&ec](const asio::error_code& e, const asio::ip::tcp::endpoint&)
    {
        ec = e;
    });

    client->io_context.restart();
    client->io_context.run_for(std::chrono::milliseconds(box->timeout));

    // If the asynchronous operation completed successfully then the io_context
    // would have been stopped due to running out of work. If it was not
    // stopped, then the io_context::run_for call must have timed out and the
    // operation is still incomplete.

    if (!client->io_context.stopped())
    {
        // Close the socket to cancel the outstanding asynchronous operation.
        client->socket.close();

        // Run the io_context again until the operation completes.
        client->io_context.run();
    }

    if (ec)
    {
        lua_pushnil(L);
        if (ec == asio::error::operation_aborted)
        {
            lua_pushliteral(L, "timeout");
        }
        else
        {
            lua_pushlstring(L, ec.message().data(), ec.message().size());
        }
        delete client;
    }
    else
    {
        lua_pushinteger(L, 1);
        box->client = client;
    }

    return lua_gettop(L) - top;
}

//only support readline readlen
static int lreceive(lua_State *L)
{
    tcp_box* box = (tcp_box*)lua_touserdata(L, 1);
    if (box == nullptr || box->client == nullptr)
        return luaL_error(L, "Invalid tcp_box pointer");

    int top = lua_gettop(L);
    auto& client = box->client;

    asio::error_code ec;

    size_t count = 0;
    size_t seek = 0;

    bool read_line = false;
    size_t readn = 0;
    /* receive new patterns */
    if (!lua_isnumber(L, 2))
    {
        const char *p = luaL_optstring(L, 2, "*l");
        if (p[0] == '*' && p[1] == 'l')
        {
            read_line = true;
        }
        else
        {
            luaL_argcheck(L, 0, 2, "invalid receive pattern");
        }
    }
    else
    {
        luaL_argcheck(L, 0, 2, "invalid receive pattern");
        readn = luaL_checkinteger(L, 2);
        luaL_argcheck(L, readn > 0, 2, "invalid receive pattern");
    }

    while (true)
    {
        const char* data = reinterpret_cast<const char*>(client->buf.data().data());
        std::string_view content{ data ,client->buf.size() };

        if (read_line)
        {
            auto pos = content.find('\n');
            if (content.find('\n') != std::string_view::npos)
            {
                count = pos;
                seek = count + 1;
                break;
            }
        }
        else
        {
            if (content.size() >= readn)
            {
                count = seek= readn;
                break;
            }
        }

        int64_t total_tm = 0;
        while (0 == client->socket.available(ec)&&!ec)
        {
            int64_t start_tm =  millisecond();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            total_tm += millisecond() - start_tm;
            if (static_cast<int64_t>(box->timeout) <= total_tm)
            {
                goto READ_DONE;
            }
        }

        // Need more data.
        std::size_t bytes_to_read = std::min<std::size_t>(
            std::max<std::size_t>(512, client->buf.capacity() - client->buf.size()),
            std::min<std::size_t>(65536, client->buf.max_size() - client->buf.size()));
        client->buf.commit(client->socket.read_some(client->buf.prepare(bytes_to_read), ec));
        if (ec)
        {
            break;
        }
    }

READ_DONE:

    if (ec)
    {
        lua_pushnil(L);
        lua_pushliteral(L, "closed");
        lua_pushnil(L);
    }
    else if(count == 0)
    {
        lua_pushnil(L);
        lua_pushliteral(L, "timeout");
        lua_pushnil(L);
    }
    else
    {
        auto data = reinterpret_cast<const char*>(client->buf.data().data());
        lua_pushlstring(L, data, count);
        lua_pushnil(L);
        lua_pushnil(L);
        client->buf.consume(seek);
    }
    return lua_gettop(L) - top;
}

static int lsend(lua_State *L)
{
    tcp_box* box = (tcp_box*)lua_touserdata(L, 1);
    if (box == nullptr || box->client == nullptr)
        return luaL_error(L, "Invalid tcp_box pointer");
    int top = lua_gettop(L);
    size_t size = 0;
    const char* data = luaL_checklstring(L, 2, &size);
    int64_t start = (int64_t)luaL_optnumber(L, 3, 1);
    int64_t end = (int64_t)luaL_optnumber(L, 4, -1);
    if (start < 0) start = (int64_t)(size + start + 1);
    if (end < 0) end = (int64_t)(size + end + 1);
    if (start < 1) start = (int64_t)1;
    if (end > (int64_t) size) end = (int64_t)size;

    auto& client = box->client;
    asio::error_code ec;

    size_t sent = 0;
    if (start <= end)
    {
        sent = asio::write(client->socket, asio::buffer(data + start - 1, end - start + 1), ec);
    }

    /* check if there was an error */
    if (ec) {
        lua_pushnil(L);
        lua_pushstring(L, ec.message().data());
        lua_pushnumber(L, (lua_Number)(sent + start - 1));
    }
    else {
        lua_pushnumber(L, (lua_Number)(sent + start - 1));
        lua_pushnil(L);
        lua_pushnil(L);
    }
    return lua_gettop(L) - top;
}

static int lclose(lua_State *L)
{
    tcp_box* box = (tcp_box*)lua_touserdata(L, 1);
    if (box == nullptr || box->client == nullptr)
        return luaL_error(L, "Invalid tcp_box pointer");
    box->client->socket.close();
    box->client->io_context.stop();
    delete box->client;
    box->client = nullptr;
    return 0;
}

static int ltcp(lua_State *L)
{
    tcp_box* box = (tcp_box*)lua_newuserdata(L, sizeof(tcp_box));
    box->client = nullptr;
    box->timeout = 0;
    if (luaL_newmetatable(L, METANAME))//mt
    {
        luaL_Reg l[] = {
            { "settimeout",lsettimeout },
            { "connect",lconnect },
            { "receive", lreceive},
            { "send", lsend},
            { "close", lclose},
            { NULL,NULL }
        };
        luaL_newlib(L, l); //{}
        lua_setfield(L, -2, "__index");//mt[__index] = {}
        lua_pushcfunction(L, lrelease);
        lua_setfield(L, -2, "__gc");//mt[__gc] = lrelease
    }
    lua_setmetatable(L, -2);// set userdata metatable
    lua_pushlightuserdata(L, box);
    return 2;
}

extern "C"
{
    int LUAMOD_API luaopen_socket_core(lua_State *L)
    {
        luaL_Reg l[] = {
            {"tcp",ltcp},
            {"release",lrelease },
            {NULL,NULL}
        };
        luaL_checkversion(L);
        luaL_newlib(L, l);
        return 1;
    }
}
