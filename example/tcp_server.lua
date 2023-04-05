local moon = require("moon")

local socket = require("moon.socket")

local conf = ...

local HOST = conf.host or "127.0.0.1"
local PORT = conf.port or 12345

-------------------2 bytes len (big endian) protocol------------------------
socket.on("accept",function(fd, msg)
    print("accept ", fd, moon.decode(msg, "Z"))
    -- 设置read超时，单位秒
    socket.settimeout(fd, 60)
    -- 该协议默认只支持最大长度65534字节的数据收发
    -- 可以设置标记允许对超过这个长度的数据包进行分包 r:表示读。w:表示写
    -- 为了防止缓冲区攻击,尽量不要允许客户端上行消息分包
    socket.set_enable_chunked(fd, "w")

    -- socket.set_enable_chunked(fd, "rw")
end)

socket.on("message",function(fd, msg)
    --echo message to client
    socket.write(fd, moon.decode(msg, "Z"))
end)

socket.on("close",function(fd, msg)
    print("close ", fd, moon.decode(msg, "Z"))
end)

-- moon.PTYPE_SOCKET_MOON ：2字节(大端)表示长度的协议
local listenfd = socket.listen(HOST, PORT, moon.PTYPE_SOCKET_MOON)
socket.start(listenfd)--start accept
print("server start ", HOST, PORT)
print("enter 'CTRL-C' stop server.")

moon.shutdown(function()
    socket.close(listenfd)
	moon.quit()
end)


