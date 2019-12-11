local moon = require("moon")

---@type socketcore
local core = require("socketcore")

local setmetatable = setmetatable
local tostring = tostring
local tointeger = math.tointeger
local yield = coroutine.yield
local make_response = moon.make_response
local id = moon.id()

core.__index = core

local accept = core.accept
local connect = core.connect
local read = core.read
local write_with_flag = core.write_with_flag

local linedelim = {}
linedelim['\r\n'] = 1
linedelim['\r\n\r\n'] = 2
linedelim['\n'] = 3

local close_flag = moon.buffer_flag.close
local ws_text_flag = moon.buffer_flag.ws_text
local ws_ping_flag = moon.buffer_flag.ws_ping
local ws_pong_flag = moon.buffer_flag.ws_pong

---@class socket : socketcore
local socket = {}

setmetatable(socket,core)

--- async
function socket.accept(listenfd, serviceid)
    serviceid = serviceid or moon.sid()
    local sessionid = make_response()
    accept(listenfd, sessionid, serviceid)
    local fd,err = yield()
    if not fd then
        return nil,err
    end
    return tointeger(fd)
end

function socket.start(listenfd)
    accept(listenfd,0,id)
end

--- async
--- param protocol moon.PTYPE_TEXT、moon.PTYPE_SOCKET、moon.PTYPE_SOCKET_WS、
--- timeout millseconds
---@param host string
---@param port integer
---@param protocol integer
---@param timeout integer
function socket.connect(host, port, protocol, timeout)
    timeout = timeout or 0
    local sessionid = make_response()
    connect(host, port, id, protocol, sessionid, timeout)
    local fd,err = yield()
    if not fd then
        return nil,err
    end
    return tointeger(fd)
end

function socket.sync_connect(host, port, protocol)
    local fd = connect(host, port, id, protocol, 0, 0)
    if fd == 0 then
        return nil,"connect failed"
    end
    return fd
end

--- async
function socket.read(fd, len)
    local sessionid = make_response()
    read(fd, id, len, 0, sessionid)
    return yield()
end

--- async
function socket.readline(fd, delim, limit)
    delim = linedelim[delim]
    if not delim  then
        error("unsupported read delim "..tostring(delim))
    end
    limit= limit or 0
    local sessionid = make_response()
    read(fd, id, limit, delim, sessionid)
    return yield()
end

function socket.write_then_close(fd, data)
    write_with_flag(fd ,data, close_flag)
end

--- only for websocket
function socket.write_text(fd, data)
    write_with_flag(fd ,data, ws_text_flag)
end

function socket.write_ping(fd, data)
    write_with_flag(fd ,data, ws_ping_flag)
end

function socket.write_pong(fd, data)
    write_with_flag(fd ,data, ws_pong_flag)
end

local socket_data_type = {
    connect = 1,
    accept = 2,
    message = 3,
    close = 4,
    error = 5,
    ping = 6,
    pong = 7,
}

--- tow bytes len protocol callbacks
local callbacks = table.new(0,6)

--- websocket protocol wscallbacks
local wscallbacks = table.new(0,6)

moon.dispatch(
    "socket",
    function(msg)
        local fd = msg:sender()
        local subtype = msg:subtype()
        local f = callbacks[subtype]
        if f then
            f(fd, msg)
        end
    end
)

moon.dispatch(
    "websocket",
    function(msg)
        local fd = msg:sender()
        local subtype = msg:subtype()
        local f = wscallbacks[subtype]
        if f then
            f(fd, msg)
        end
    end
)

---param name socket_data_type's key
---@param name string
function socket.on(name, cb)
    local n = socket_data_type[name]
    if n then
        callbacks[n] = cb
    else
        error("register unsupport socket data type "..name)
    end
end

---param name socket_data_type's key
---@param name string
function socket.wson(name, cb)
    local n = socket_data_type[name]
    if n then
        wscallbacks[n] = cb
    else
        error("register unsupport websocket data type "..name)
    end
end

return socket
