--Echo Server Example
local moon = require("moon")

local socket = require("moon.socket")

local conf = ...

local HOST = conf.host or "127.0.0.1"
local PORT = conf.port or 12346

socket.wson("accept",function(fd, msg)
    print("wsaccept ", fd, msg:bytes())
    -- 设置read超时，单位秒
    socket.settimeout(fd, 10)
end)

socket.wson("message",function(fd, msg)
    -- binary frame
    -- socket.write(fd, msg)
    -- text frame
    socket.write_text(fd, msg:bytes())
end)

socket.wson("close",function(fd, msg)
    print("wsclose ", fd, msg:bytes())
end)

socket.wson("error",function(fd, msg)
    print("wserror ", fd, msg:bytes())
end)

socket.wson("ping",function(fd, msg)
    print("wsping ", fd, msg:bytes())
    socket.write_pong(fd,"my pong")
end)

local listenfd = 0
moon.start(function()
    listenfd = socket.listen(HOST, PORT, moon.PTYPE_SOCKET_WS)
    socket.start(listenfd)
    print("websocket server start ", HOST, PORT)
    print("enter 'CTRL-C' stop server.")
end)

moon.destroy(function()
    socket.close(listenfd)
end)

