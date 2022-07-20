local moon = require("moon")

local core = require("asio")

local tointeger = math.tointeger
local yield = coroutine.yield
local make_session = moon.make_session
local id = moon.id

local close = core.close
local accept = core.accept
local connect = core.connect
local read = core.read
local write = core.write
local udp = core.udp
local unpack_udp = core.unpack_udp

local flag_close = 2
local flag_ws_text = 16
local flag_ws_ping = 32
local flag_ws_pong = 64

local supported_tcp_protocol = {
    [moon.PTYPE_SOCKET_TCP] = true,
    [moon.PTYPE_SOCKET_WS] = true,
    [moon.PTYPE_SOCKET_MOON] = true,
}

---@class socket : asio
local socket = core

--- async
function socket.accept(listenfd, serviceid)
    serviceid = serviceid or id
    local sessionid = make_session()
    accept(listenfd, sessionid, serviceid)
    local fd,err = yield()
    if not fd then
        return nil,err
    end
    return tointeger(fd)
end

function socket.start(listenfd)
    accept(listenfd, 0, id)
end

--- async
--- param protocol moon.PTYPE_SOCKET_TCP, moon.PTYPE_SOCKET_MOON, moon.PTYPE_SOCKET_WS、
--- timeout millseconds
---@param host string
---@param port integer
---@param protocol integer
---@param timeout? integer
function socket.connect(host, port, protocol, timeout)
    assert(supported_tcp_protocol[protocol],"not support")
    timeout = timeout or 0
    local sessionid = make_session()
    connect(host, port, protocol, sessionid, timeout)
    local fd,err = yield()
    if not fd then
        return nil,err
    end
    return tointeger(fd)
end

function socket.sync_connect(host, port, protocol)
    assert(supported_tcp_protocol[protocol],"not support")
    local fd = connect(host, port, protocol, 0, 0)
    if fd == 0 then
        return nil,"connect failed"
    end
    return fd
end

--- async, used only when protocol == moon.PTYPE_SOCKET_TCP
---@param delim string @read until reach the specified delim string from the socket
---@param maxcount? integer
---@overload fun(fd: integer, count: integer) @ read a specified number of bytes from the socket.
function socket.read(fd, delim, maxcount)
    local sessionid = make_session()
    read(fd, sessionid, delim, maxcount)
    return yield()
end

function socket.write_then_close(fd, data)
    write(fd ,data, flag_close)
end

--- PTYPE_SOCKET_WS specific functions
function socket.write_text(fd, data)
    write(fd ,data, flag_ws_text)
end

--- PTYPE_SOCKET_WS specific functions
function socket.write_ping(fd, data)
    write(fd ,data, flag_ws_ping)
end

--- PTYPE_SOCKET_WS specific functions
function socket.write_pong(fd, data)
    write(fd ,data, flag_ws_pong)
end

local socket_data_type = {
    connect = 1,
    accept = 2,
    message = 3,
    close = 4,
    ping = 5,
    pong = 6,
}

---@alias socket_event
---| 'connect'
---| 'accept'
---| 'message'
---| 'close'
---| 'ping'
---| 'pong'

--- PTYPE_SOCKET_MOON callbacks
local callbacks = {}

--- PTYPE_SOCKET_WS wscallbacks
local wscallbacks = {}

local _decode = moon.decode

moon.dispatch(
    "moonsocket",
    function(msg)
        local fd, sdt = _decode(msg, "SR")
        local f = callbacks[sdt]
        if f then
            f(fd, msg)
        end
    end
)

moon.dispatch(
    "websocket",
    function(msg)
        local fd, sdt = _decode(msg, "SR")
        local f = wscallbacks[sdt]
        if f then
            f(fd, msg)
        end
    end
)

---@param name socket_event
---@param cb fun(fd:integer, msg:message_ptr)
function socket.on(name, cb)
    local n = socket_data_type[name]
    if n then
        callbacks[n] = cb
    else
        error("register unsupport socket data type "..name)
    end
end

---@param name socket_event
---@param cb fun(fd:integer, msg:message_ptr)
function socket.wson(name, cb)
    local n = socket_data_type[name]
    if n then
        wscallbacks[n] = cb
    else
        error("register unsupport websocket data type "..name)
    end
end

local udp_callbacks = {}

moon.dispatch(
    "udp",
    function(msg)
        local fd, p, n = _decode(msg, "SC")
        local fn = udp_callbacks[fd]
        if not fn then
            moon.error("drop udp message from", fd)
            return
        end
        local from, str = unpack_udp(p, n)
        fn(str, from)
    end
)

---@param cb fun(data:string, endpoint:string)
---@param host? string @ bind host
---@param port? integer @ bind port
function socket.udp(cb, host, port)
    local fd = udp(host, port)
    udp_callbacks[fd] = cb
    return fd
end

function socket.close(fd)
    close(fd)
    udp_callbacks[fd] = nil
end

return socket
