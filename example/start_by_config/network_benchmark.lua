local moon = require("moon")
local socket = require("moon.socket")
local seri = require("seri")
local binunpack = seri.unpack
local binpack = seri.pack

local conf = ... or {}

socket.on("accept",function(fd, msg)
    --print("accept ", fd, moon.decode(msg, "Z"))
end)

socket.on("message",function(fd, msg)
    socket.write(fd, moon.decode(msg, "Z"))
end)

socket.on("close",function(fd, msg)
    --print("close ", fd, moon.decode(msg, "Z"))
end)



local listenfd  = socket.listen(conf.host,conf.port,moon.PTYPE_SOCKET_MOON)
socket.start(listenfd)

print(string.format([[

    network benchmark run at %s %d with %d slaves.
]], conf.host, conf.port, conf.count))

