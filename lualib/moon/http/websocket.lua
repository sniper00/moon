local crypt = require("crypt")
local moon  = require("moon")
local socket = require("moon.socket")
local internal = require("moon.http.internal")

local WS_MAGICKEY = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

--- config.hpp: socket_send_mask
local flag_ws_text = 4
local flag_ws_ping = 8
local flag_ws_pong = 16

--- PTYPE_SOCKET_WS wscallbacks
local wscallbacks = {}

local _decode = moon.decode

local socket_data_type = {
    connect = 1,
    accept = 2,
    message = 3,
    close = 4,
    ping = 5,
    pong = 6,
}

---@alias websocket_event
---| 'message'
---| 'close'
---| 'ping'
---| 'pong'

local websocket = {}

function websocket.close(fd)
    socket.close(fd)
end

function websocket.connect(url, header, timeout)
    local protocol, host, uri = string.match(url, "^(ws)://([^/]+)(.*)$")
    if protocol ~= "ws" then
        error(string.format("invalid protocol: %s", protocol))
    end
    assert(host)
    local host_addr, host_port = string.match(host, "^([^:]+):?(%d*)$")
    assert(host_addr and host_port)

    local key = crypt.base64encode(crypt.randomkey()..crypt.randomkey())

    local request_header = {
        ["Upgrade"] = "websocket",
        ["Connection"] = "Upgrade",
        ["Sec-WebSocket-Version"] = "13",
        ["Sec-WebSocket-Key"] = key
    }

    if header then
        for k,v in pairs(header) do
            assert(request_header[k] == nil, k)
            request_header[k] = v
        end
    end

    local response = internal.request("GET", host, {
        path = uri,
        connect_timeout = timeout,
        header = request_header
    })

    local recvheader = response.header

    if response.status_code ~= 101 then
        error(string.format("websocket handshake error: code[%s] info:%s", response.status_code, response.content))
    end
	--assert(response.content == "")	-- todo: M.read may need handle it

    if not recvheader["upgrade"] or recvheader["upgrade"]:lower() ~= "websocket" then
        error("websocket handshake upgrade must websocket")
    end

    if not recvheader["connection"] or recvheader["connection"]:lower() ~= "upgrade" then
        error("websocket handshake connection must upgrade")
    end

    local sw_key = recvheader["sec-websocket-accept"]
    if not sw_key then
        error("websocket handshake need Sec-WebSocket-Accept")
    end

    sw_key = crypt.base64decode(sw_key)
    if sw_key ~= crypt.sha1(key .. WS_MAGICKEY) then
        error("websocket handshake invalid Sec-WebSocket-Accept")
    end

    socket.switch_type(response.socket_fd, moon.PTYPE_SOCKET_WS);

    return response.socket_fd
end

function websocket.listen(host, port)
    local fd = socket.listen(host, port, moon.PTYPE_SOCKET_WS)
    assert(fd>0)
    socket.start(fd)
    return fd
end

---@param cb fun(fd:integer, HttpRequest)
function websocket.on_accept(cb)
    wscallbacks[socket_data_type.accept] = function (fd, msg)
        local header = internal.parse_header(moon.decode(msg, "Z"))
        cb(fd, header)
    end
end

---@param name websocket_event
---@param cb fun(fd:integer, msg:message_ptr)
function websocket.wson(name, cb)
    local n = socket_data_type[name]
    if n then
        wscallbacks[n] = cb
    else
        error("register unsupport websocket data type " .. name)
    end
end

--- NOTE:  PTYPE_SOCKET_WS specific functions
function websocket.write_text(fd, data)
    return socket.write(fd, data, flag_ws_text)
end

--- NOTE:  PTYPE_SOCKET_WS specific functions
function websocket.write_ping(fd, data)
    return socket.write(fd, data, flag_ws_ping)
end

--- NOTE:  PTYPE_SOCKET_WS specific functions
function websocket.write_pong(fd, data)
    return socket.write(fd, data, flag_ws_pong)
end

moon.raw_dispatch(
    "websocket",
    function(msg)
        local fd, sdt = _decode(msg, "SR")
        local f = wscallbacks[sdt]
        if f then
            f(fd, msg)
        end
    end
)

return websocket