local moon = require("moon")

local socket = require("moon.socket")

local HOST = "127.0.0.1"
local PORT = 12345

-------------------2 bytes len (big endian) protocol------------------------
socket.on("accept",function(fd, msg)
    print("accept ", fd, moon.decode(msg, "Z"))
    socket.settimeout(fd, 10)
end)

socket.on("message",function(fd, msg)
    --echo message to client
    socket.write(fd, moon.decode(msg, "Z"))
end)

socket.on("close",function(fd, msg)
    print("close ", fd, moon.decode(msg, "Z"))
end)

local listenfd = socket.listen(HOST, PORT, moon.PTYPE_SOCKET_MOON)
socket.start(listenfd)--start accept
print("server start ", HOST, PORT)
print("enter 'CTRL-C' stop server.")

moon.shutdown(function()
    socket.close(listenfd)
	moon.quit()
end)