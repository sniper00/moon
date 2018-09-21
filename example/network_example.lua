local moon = require("moon")
local tcpserver = require("moon.tcpserver")

--Echo Server Example

tcpserver.on("accept",function(sessionid, msg)
    print("accept ", sessionid, msg:bytes())
end)

local response = string.pack("H", 2)
tcpserver.on("message",function(sessionid, msg)
    tcpserver.send(sessionid, response .. msg:bytes())
end)

tcpserver.on("close",function(sessionid, msg)
    print("close ", sessionid, msg:bytes())
end)

tcpserver.on("error",function(sessionid, msg)
    print("error ", sessionid, msg:bytes())
end)
