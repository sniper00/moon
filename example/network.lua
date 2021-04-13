local moon = require("moon")

local socket = require("moon.socket")

local conf = ... or {}

local HOST = conf.host or "127.0.0.1"
local PORT = conf.port or 12345

-------------------2 bytes len (big endian) protocol------------------------
socket.on("accept",function(fd, msg)
    print("accept ", fd, moon.decode(msg, "Z"))
    -- 设置read超时，单位秒
    socket.settimeout(fd, 10)
    -- 该协议默认只支持最大长度32KB的数据收发
    -- 设置标记可以把超多32KB的数据拆分成多个包
    -- 可以单独设置独写的标记，r:表示读。w:表示写
    socket.set_enable_chunked(fd, "w")
end)

socket.on("message",function(fd, msg)
    --echo message to client
    socket.write(fd, moon.decode(msg, "Z"))
end)

socket.on("close",function(fd, msg)
    print("close ", fd, moon.decode(msg, "Z"))
end)

socket.on("error",function(fd, msg)
    print("error ", fd, moon.decode(msg, "Z"))
end)

-- moon.PTYPE_SOCKET ：2字节(大端)表示长度的协议
local listenfd = socket.listen(HOST, PORT, moon.PTYPE_SOCKET)
socket.start(listenfd)--start accept
print("server start ", HOST, PORT)
print("enter 'CTRL-C' stop server.")

moon.shutdown(function()
    socket.close(listenfd)
	moon.quit()
end)


