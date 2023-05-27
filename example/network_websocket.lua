--Echo Server Example
local moon = require("moon")
local socket = require("moon.socket")
local http_server = require("moon.http.server")

local conf = ...

local HOST = conf.host or "127.0.0.1"
local PORT = conf.port or 12346

socket.wson("accept",function(fd, msg)
    local header = http_server.parse_header(moon.decode(msg, "Z"))
    print_r(header)

    print("wsaccept ", fd)
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

socket.wson("ping",function(fd, msg)
    print("wsping ", fd, moon.decode(msg, "Z"))
    socket.write_pong(fd,"my pong")
end)

local listenfd = socket.listen(HOST, PORT, moon.PTYPE_SOCKET_WS)
socket.start(listenfd)
print("websocket server start ", HOST, PORT)
print("enter 'CTRL-C' stop server.")


moon.async(function ()
    local fd = socket.connect(HOST, PORT, "ws", 0, "chat")
    socket.write(fd, "hello world");
end)

moon.shutdown(function()
    socket.close(listenfd)
	moon.quit()
end)


