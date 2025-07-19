local moon = require("moon")
local core = require("asio.core")

local id = moon.id

local close = core.close
local accept = core.accept
local connect = core.connect
local read = core.read
local write = core.write
local udp = core.udp
local unpack_udp = core.unpack_udp

---@type integer
local mask_close<const> = 2
---@type integer
local mask_raw<const> = 32

---@type table<string|integer, string|integer>
local supported_tcp_protocol = {
    [moon.PTYPE_SOCKET_TCP] = "tcp",
    [moon.PTYPE_SOCKET_MOON] = "moon",
    tcp = moon.PTYPE_SOCKET_TCP,
    moon = moon.PTYPE_SOCKET_MOON
}

---@class socket : asio
---@field mask_raw integer Raw data mask for socket operations
local socket = core

socket.mask_raw = mask_raw

---Accept a new connection on a listening socket
---@async
---@param listenfd integer The listening socket file descriptor
---@param serviceid? integer The service ID to handle the connection, defaults to current service ID
---@return integer|nil fd The accepted socket file descriptor, or nil on error
---@return string? err Error message if the operation failed
function socket.accept(listenfd, serviceid)
    assert(listenfd>0, "Invalid listenfd")
    serviceid = serviceid or id
    local fd, err = moon.wait(accept(listenfd, serviceid))
    if not fd then
        return nil, err
    end
    return fd
end

---Start accepting connections on a listening socket
---@param listenfd integer The listening socket file descriptor
function socket.start(listenfd)
    assert(listenfd>0, "Invalid listenfd")
    accept(listenfd, id, 0)
end

---Connect to a remote host and port
---@async
---@param host string The hostname or IP address to connect to
---@param port integer The port number to connect to
---@param protocol integer|string The protocol to use ("tcp", "moon", or protocol constants)
---@param timeout? integer Connection timeout in milliseconds, defaults to 0 (no timeout)
---@return integer|nil fd The connected socket file descriptor, or nil on error
---@return string? err Error message if the connection failed
function socket.connect(host, port, protocol, timeout)
    assert(supported_tcp_protocol[protocol], "not support")
    if type(protocol) == "string" then
        protocol = supported_tcp_protocol[protocol]
    end
    timeout = timeout or 0
    local fd, err = moon.wait(connect(host, port, protocol, timeout))
    if not fd then
        return nil, err
    end
    return fd
end

---Read data from a socket
--- NOTE: Used only when protocol == moon.PTYPE_SOCKET_TCP
---@async
---@param fd integer The socket file descriptor
---@param delim string Read until reaching the specified delimiter string from the socket. Max length is 7 bytes.
---@param maxcount? integer Maximum number of bytes to read
---@return string|nil data The read data, or nil on error
---@return string? err Error message if the read failed
---@overload fun(fd: integer, count: integer): string|nil, string? Read a specified number of bytes from the socket
function socket.read(fd, delim, maxcount)
    local session, data = read(fd, delim, maxcount)
    if session and data then
        return data
    end
    return moon.wait(session, data)
end

---Write data to a socket and then close it
---@param fd integer The socket file descriptor
---@param data string|buffer_ptr|buffer_shr_ptr The data to write before closing
function socket.write_then_close(fd, data)
    write(fd, data, mask_close)
end

---Write raw data to a socket, bypassing any message encoding
--- This function sends raw network data, bypassing any message encoding.
--- If you need to send data that must be encoded in a specific way, you should encode the data before calling this function.
---@param fd integer The socket file descriptor
---@param data string|buffer_ptr|buffer_shr_ptr The data to be written. This can be a string, a buffer pointer, or a shared buffer pointer.
function socket.write_raw(fd, data)
    write(fd, data, mask_raw)
end

---@type table<string, integer>
local socket_data_type = {
    connect = 1,
    accept = 2,
    message = 3,
    close = 4,
}

---@alias socket_event
---| 'connect' # Socket connection established
---| 'accept'  # New connection accepted
---| 'message' # Data message received
---| 'close'   # Socket closed

--- PTYPE_SOCKET_MOON callbacks
---@type table<integer, fun(fd: integer, msg: message_ptr)>
local callbacks = {}

local _decode = moon.decode

moon.raw_dispatch(
    "moonsocket",
    function(msg)
        local fd, sdt = _decode(msg, "SR")
        local f = callbacks[sdt]
        if f then
            f(fd, msg)
        end
    end
)

---Register a callback for socket events
--- NOTE: Used only when protocol == moon.PTYPE_SOCKET_MOON
---@param name socket_event The socket event type to register for
---@param cb fun(fd: integer, msg: message_ptr) The callback function to handle the event
function socket.on(name, cb)
    local n = socket_data_type[name]
    if n then
        callbacks[n] = cb
    else
        error("register unsupport socket data type " .. name)
    end
end

---@type table<integer, fun(data: string, endpoint: string)>
local udp_callbacks = {}

moon.raw_dispatch(
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

---Create a UDP socket and register a callback for incoming data
---@param cb fun(data: string, endpoint: string) Callback function to handle incoming UDP data
---@param host? string Bind host address, optional
---@param port? integer Bind port number, optional
---@return integer fd The UDP socket file descriptor
function socket.udp(cb, host, port)
    local fd = udp(host, port)
    udp_callbacks[fd] = cb
    return fd
end

---Close a socket and clean up associated resources
---@param fd integer The socket file descriptor to close
function socket.close(fd)
    close(fd)
    udp_callbacks[fd] = nil
end

return socket
