--Echo Server Example
local moon = require("moon")
local socket = require("moon.socket")
local websocket = require("moon.http.websocket")

local conf = ...

local HOST = conf.host or "127.0.0.1"
local PORT = conf.port or 12346

websocket.on_accept(function(fd, request)
    print_r(request)

    print("wsaccept ", fd)
    -- 设置read超时，单位秒
    socket.settimeout(fd, 10)
end)

websocket.wson("message",function(fd, msg)
    print(moon.decode(msg, "Z"))

    -- binary frame
    -- websocket.write(fd, msg)

    -- text frame
    websocket.write_text(fd, moon.decode(msg, "Z"))
end)

websocket.wson("close",function(fd, msg)
    print("wsclose ", fd, moon.decode(msg, "Z"))
end)

websocket.wson("ping",function(fd, msg)
    print("wsping ", fd, moon.decode(msg, "Z"))
    websocket.write_pong(fd,"my pong")
end)

local listenfd = websocket.listen(HOST, PORT)
print("websocket server start ", HOST, PORT)
print("enter 'CTRL-C' stop server.")

moon.async(function ()
    local fd = websocket.connect(string.format("ws://%s:%s/chat", HOST, PORT))
    assert(websocket.write_text(fd, "hello world"))
    websocket.close(fd)
end)

moon.shutdown(function()
    websocket.close(listenfd)
	moon.quit()
end)


