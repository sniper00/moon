local moon = require("moon")

local socket = require("moon.socket")

local HOST = "127.0.0.1"
local PORT = 9526

-------------------2 bytes len (big endian) protocol------------------------
socket.on("accept",function(fd, msg)
    print("accept ", fd, msg:bytes())
    socket.settimeout(fd, 10)
end)

socket.on("message",function(fd, msg)
    --echo message to client
    socket.write(fd, msg:bytes())
end)

socket.on("close",function(fd, msg)
    print("close ", fd, msg:bytes())
end)

socket.on("error",function(fd, msg)
    print("error ", fd, msg:bytes())
end)

local listenfd = 0

moon.start(function()
    listenfd = socket.listen(HOST, PORT, moon.PTYPE_SOCKET)
    socket.start(listenfd)--start accept
    print("server start ", HOST, PORT)
    print("enter 'CTRL-C' stop server.")
end)

-- 服务销毁
moon.destroy(function()
    socket.close(listenfd)
end)
