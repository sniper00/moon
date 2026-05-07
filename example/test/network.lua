local moon = require("moon")
local socket = require("moon.socket")
local core = require("asio.core")
local test_assert = require("test_assert")

local HOST = "127.0.0.1"
local PORT = 30003
--------------------------SERVER-------------------------

local listenfd = 0
while listenfd == 0 do
    listenfd = socket.listen(HOST, PORT, moon.PTYPE_SOCKET_MOON)
    if listenfd == 0 then
        PORT = PORT + 1
    end
end

test_assert.assert(listenfd > 0, "listen should return valid fd")
test_assert.equal(core.try_open(HOST, PORT, false), false)

local can_connect = core.try_open(HOST, PORT, true)
test_assert.assert(type(can_connect) == "boolean", "try_open(connect) should return boolean")

local address = core.getaddress(listenfd)
test_assert.assert(type(address) == "string", "getaddress should return string")

local endpoint, err = core.make_endpoint(HOST, PORT)
test_assert.assert(type(endpoint) == "string" and #endpoint > 0, "make_endpoint should encode valid address")

local ok, invalid_err = core.make_endpoint("not-an-ip", PORT)
test_assert.assert(not ok and type(invalid_err) == "string", "make_endpoint should reject invalid ip")

ok = pcall(socket.connect, HOST, PORT, "invalid-protocol")
test_assert.assert(not ok, "socket.connect should reject unsupported protocol")

ok = pcall(core.write, listenfd, "x", 64)
test_assert.assert(not ok, "asio.write should reject invalid mask")

socket.start(listenfd)

socket.on("accept",function(fd, msg)
    --print("accept ", fd, moon.decode(msg, "Z"))
end)

socket.on("message",function(fd, msg)
    socket.write_message(fd, msg)
end)

socket.on("close",function(fd, msg)
    --print("close ", fd, moon.decode(msg, "Z"))
end)

------------------------CLIENT----------------------------

local function send(fd,data)
    if not fd then
        return false
    end
    local len = #data
    return socket.write(fd, string.pack(">H",len)..data)
end

local function session_read( fd )
    if not fd then
        return false
    end
    local data,err = socket.read(fd, 2)
    if not data then
        print(fd,"fd read error",err)
        return false
    end

    local len = string.unpack(">H",data)

    data,err = socket.read(fd, len)
    if not data then
        print(fd,"fd read error",err)
        return false
    end
    return data
end

moon.async(function()
    for i=1,100 do
        local fd,err = socket.connect(HOST,PORT,moon.PTYPE_SOCKET_TCP)
        if not fd then
            print("connect failed", err)
            return
        end
        moon.async(function ()
            local send_data = tostring(fd)
            send(fd, send_data)
            local rdata = session_read(fd)
            test_assert.equal(rdata, send_data)
            socket.close(fd)
            if i == 100 then
                socket.close(listenfd)
                test_assert.success()
            end
        end)
    end
end)

