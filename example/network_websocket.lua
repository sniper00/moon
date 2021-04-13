--Echo Server Example
local moon = require("moon")

local socket = require("moon.socket")

local conf = ... or {}

local HOST = conf.host or "127.0.0.1"
local PORT = conf.port or 12346

socket.wson("accept",function(fd, msg)
    print("wsaccept ", fd, moon.decode(msg, "Z"))
    -- 设置read超时，单位秒
    socket.settimeout(fd, 10)
end)

socket.wson("message",function(fd, msg)
    -- binary frame
    -- socket.write(fd, msg)
    -- text frame
    socket.write_text(fd, moon.decode(msg, "Z"))
end)

socket.wson("close",function(fd, msg)
    print("wsclose ", fd, moon.decode(msg, "Z"))
end)

socket.wson("error",function(fd, msg)
    print("wserror ", fd, moon.decode(msg, "Z"))
end)

socket.wson("ping",function(fd, msg)
    print("wsping ", fd, moon.decode(msg, "Z"))
    socket.write_pong(fd,"my pong")
end)

local listenfd = socket.listen(HOST, PORT, moon.PTYPE_SOCKET_WS)
socket.start(listenfd)
print("websocket server start ", HOST, PORT)
print("enter 'CTRL-C' stop server.")

moon.shutdown(function()
    socket.close(listenfd)
	moon.quit()
end)


