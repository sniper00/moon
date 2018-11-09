--Echo Server Example

do
    -------------------2 bytes len (big endian) protocol------------------------
    local tcpserver = require("moon.tcpserver")

    tcpserver.on("accept",function(sessionid, msg)
        print("accept ", sessionid, msg:bytes())
    end)

    tcpserver.on("message",function(sessionid, msg)
        --tcpserver.send(sessionid, msg:bytes())
        tcpserver.send_message(sessionid, msg)
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
    local websocket = require("moon.websocket")

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

