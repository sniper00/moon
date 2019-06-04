--Echo Server Example
local moon = require("moon")

local socket = require("moon.socket")

do
    -------------------2 bytes len (big endian) protocol------------------------
    socket.on("accept",function(fd, msg)
        print("accept ", fd, msg:bytes())
        --socket.settimeout(fd, 10)
    end)

    socket.on("message",function(fd, msg)
        --socket.write(fd, msg:bytes())
        socket.write_message(fd, msg)
    end)

    socket.on("close",function(fd, msg)
        print("close ", fd, msg:bytes())
    end)

    socket.on("error",function(fd, msg)
        print("error ", fd, msg:bytes())
    end)
end

do
    socket.wson("accept",function(fd, msg)
        print("wsaccept ", fd, msg:bytes())
    end)

    socket.wson("message",function(fd, msg)
        --socket.write(fd, msg)
        socket.write_text(fd, msg:bytes())
    end)

    socket.wson("close",function(fd, msg)
        print("wsclose ", fd, msg:bytes())
    end)

    socket.wson("error",function(fd, msg)
        print("wserror ", fd, msg:bytes())
    end)
end

moon.init(function(conf)
    local listenfd1 = socket.listen(conf.network.host,conf.network.port,moon.PTYPE_SOCKET)
    socket.start(listenfd1)
    print(string.format([[

        Tow bytes(big endian) len protocol server start:%s %d.
    ]], conf.network.host,conf.network.port))
    local wslistenfd = socket.listen(conf.networkws.host,conf.networkws.port,moon.PTYPE_SOCKET_WS)
    print(wslistenfd)
    socket.start(wslistenfd)
    print(string.format([[

        websocket protocol server start:%s %d.
    ]], conf.networkws.host,conf.networkws.port))
    return true
end)

