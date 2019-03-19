--Echo Server Example

do
    -------------------2 bytes len (big endian) protocol------------------------
    local tcpserver = require("moon.net.tcpserver")

    tcpserver.settimeout(10)

    tcpserver.on("accept",function(sessionid, msg)
        print("accept ", sessionid, msg:bytes())
    end)

    tcpserver.on("message",function(sessionid, msg)
        --tcpserver.send(sessionid, msg:bytes())
        tcpserver.send_text(sessionid, msg:bytes())
    end)

    tcpserver.on("close",function(sessionid, msg)
        print("close ", sessionid, msg:bytes())
    end)

    tcpserver.on("error",function(sessionid, msg)
        print("error ", sessionid, msg:bytes())
    end)
end

do
-------------------WEBSOCKET------------------------
    local websocket = require("moon.net.websocket")

    websocket.settimeout(10)

    websocket.on("accept",function(sessionid, msg)
        print("wsaccept ", sessionid, msg:bytes())
    end)

    websocket.on("message",function(sessionid, msg)
        --websocket.send(sessionid, msg:bytes())
        websocket.send_message(sessionid, msg)
    end)

    websocket.on("close",function(sessionid, msg)
        print("wsclose ", sessionid, msg:bytes())
    end)

    websocket.on("error",function(sessionid, msg)
        print("wserror ", sessionid, msg:bytes())
    end)
end

