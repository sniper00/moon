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
    bool isreading = false;
    std::size_t readn = 0;
    asio::streambuf buf;
    asio::error_code error;
    std::shared_ptr<asio::io_context> io_context;
    tcp::socket socket;

    tcp_client(std::shared_ptr<asio::io_context> ioctx)
        :io_context(ioctx)
        , socket(*io_context.get())
    {

    }

    ~tcp_client()
    {
        close();
    }

    void close()
    {
        if (socket.is_open())
        {
            asio::error_code ignore_ec;
            socket.shutdown(asio::ip::tcp::socket::shutdown_both, ignore_ec);
            socket.close(ignore_ec);
        }
    }
};

struct asio_context
{
    size_t timeout = 0;
    std::shared_ptr<asio::io_context>  io_context = std::make_shared<asio::io_context>();
    std::shared_ptr<tcp_client> client;

    asio::io_context& io_ctx()
    {
        return *io_context.get();
    }
};

struct master_box
{
    asio_context* asio_ctx;
};

static int lsettimeout(lua_State *L)
{
    master_box* box = (master_box*)lua_touserdata(L, 1);
    if (box == NULL || box->asio_ctx == NULL)
        return luaL_error(L, "Invalid master pointer");
    double v = lua_tonumber(L, 2);//sec

    box->asio_ctx->timeout = static_cast<size_t>(v * 1000);//millsec
    if (box->asio_ctx->timeout == 0)
    {
        box->asio_ctx->timeout = 1;
    }
    return 0;
}

static int lconnect(lua_State *L)
{
    master_box* box = (master_box*)lua_touserdata(L, 1);
    if (box == NULL || box->asio_ctx == NULL)
        return luaL_error(L, "Invalid master pointer");

    auto asio_ctx = box->asio_ctx;

    if (asio_ctx->client)
    {
        return luaL_error(L, "already connected");
    }

    size_t len = 0;
    const char* host = luaL_checklstring(L, 2, &len);
    int port = (int)luaL_checkinteger(L, 3);

    int top = lua_gettop(L);

    asio::error_code ec;

    tcp::resolver resolver(asio_ctx->io_ctx());
    asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port), ec);
    if (ec)
    {
        lua_pushnil(L);
        lua_pushlstring(L, ec.message().data(), ec.message().size());
        return 2;
    }

    std::shared_ptr<tcp_client> client = std::make_shared<tcp_client>(asio_ctx->io_context);
    asio::async_connect(client->socket, endpoints,
        [&ec](const asio::error_code& e, const asio::ip::tcp::endpoint&)
    {
        ec = e;
    });

    client->io_context->restart();

    if (asio_ctx->timeout > 0)
    {
        client->io_context->run_for(std::chrono::milliseconds(asio_ctx->timeout));
    }
    else
    {
        client->io_context->run();
    }


    // If the asynchronous operation completed successfully then the io_context
    // would have been stopped due to running out of work. If it was not
    // stopped, then the io_context::run_for call must have timed out and the
    // operation is still incomplete.

    if (!client->io_context->stopped())
    {
        // Close the socket to cancel the outstanding asynchronous operation.
        client->socket.close();
        // Run the io_context again until the operation completes.
        client->io_context->run();
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
    }
    else
    {
        lua_pushinteger(L, 1);
        asio_ctx->client = client;
    }

    return lua_gettop(L) - top;
}

static int lreceive(lua_State *L)
{
    master_box* box = (master_box*)lua_touserdata(L, 1);
    if (box == nullptr || box->asio_ctx == nullptr)
        return luaL_error(L, "Invalid tcp_box pointer");

    if (!box->asio_ctx->client)
    {
        return luaL_error(L, "socket not opened");
    }

    int top = lua_gettop(L);
    auto& client = box->asio_ctx->client;

    size_t readn = 0;
    std::string delim;
    /* receive new patterns */
    if (!lua_isnumber(L, 2))
    {
        delim = luaL_optstring(L, 2, "*l");
        if (delim == "*l")
        {
            delim = "\n";
        }
    }
    else
    {
        readn = luaL_checkinteger(L, 2);
        luaL_argcheck(L, readn > 0, 2, "invalid receive pattern");
    }

    if (!client->isreading)
    {
        client->isreading = true;
        if (!delim.empty())
        {
            asio::async_read_until(client->socket, client->buf, delim,
                [client](const asio::error_code& result_error, std::size_t result_n)
                {
                    client->error = result_error;
                    client->readn = result_n;
                    client->isreading = false;
                }
            );
        }
        else
        {
            std::size_t size = (client->buf.size() >= readn ? 0 : (readn - client->buf.size()));
            asio::async_read(client->socket, client->buf, asio::transfer_exactly(size),
                [client](const asio::error_code& result_error, std::size_t result_n)
                {
                    client->error = result_error;
                    client->readn = result_n;
                    client->isreading = false;
                }
            );
        }
    }
   
    if (client->io_context->stopped())
    {
        client->io_context->restart();
    }
    client->io_context->run_for(std::chrono::milliseconds(box->asio_ctx->timeout));

    //not timeout
    if (client->io_context->stopped())
    {
        if (client->error)
        {
            box->asio_ctx->client.reset();
            lua_pushnil(L);
            lua_pushliteral(L, "closed");
            lua_pushnil(L);
        }
        else
        {
            auto data = reinterpret_cast<const char*>(client->buf.data().data());
            if (!delim.empty())
            {
                size_t len = client->readn;
                len -= delim.size();
                lua_pushlstring(L, data, len);
            }
            else
            {
                lua_pushlstring(L, data, client->readn);
            }
            lua_pushnil(L);
            lua_pushnil(L);
            client->buf.consume(client->readn);
        }
    }
    else
    {
        lua_pushnil(L);
        lua_pushliteral(L, "timeout");
        lua_pushnil(L);
    }
    return lua_gettop(L) - top;
}

static int lsend(lua_State *L)
{
    master_box* box = (master_box*)lua_touserdata(L, 1);
    if (box == nullptr || box->asio_ctx == nullptr)
        return luaL_error(L, "Invalid tcp_box pointer");

    if (!box->asio_ctx->client)
    {
        return luaL_error(L, "socket not opened");
    }

    int top = lua_gettop(L);
    size_t size = 0;
    const char* data = luaL_checklstring(L, 2, &size);
    int64_t start = (int64_t)luaL_optnumber(L, 3, 1);
    int64_t end = (int64_t)luaL_optnumber(L, 4, -1);
    if (start < 0) start = (int64_t)(size + start + 1);
    if (end < 0) end = (int64_t)(size + end + 1);
    if (start < 1) start = (int64_t)1;
    if (end > (int64_t) size) end = (int64_t)size;

    auto& client = box->asio_ctx->client;
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
    master_box* box = (master_box*)lua_touserdata(L, 1);
    if (box == nullptr || box->asio_ctx == nullptr)
        return luaL_error(L, "Invalid tcp_box pointer");
    if (box->asio_ctx->client)
    {
        box->asio_ctx->client.reset();
    }
    box->asio_ctx->io_context->stop();
    box->asio_ctx->io_context->run();
    return 0;
}

static int lrelease(lua_State* L)
{
    master_box* box = (master_box*)lua_touserdata(L, 1);
    if (box && box->asio_ctx)
    {
        box->asio_ctx->io_context->stop();
        // Run the io_context again until the operation completes.
        box->asio_ctx->io_context->run();
        delete box->asio_ctx;
        box->asio_ctx = nullptr;
    }
    return 0;
}

static int ltcp(lua_State *L)
{
    master_box* box = (master_box*)lua_newuserdatauv(L, sizeof(master_box), 0);
    box->asio_ctx = new asio_context{};
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
    return 1;
}

int LUAMOD_API luaopen_socket_core(lua_State *L)
{
    luaL_Reg l[] = {
        {"tcp",ltcp},
        {"release",lrelease },
        {NULL,NULL}
    };
    luaL_newlib(L, l);
    return 1;
}
