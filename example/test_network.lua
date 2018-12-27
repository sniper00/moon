local moon = require("moon")
local tcpserver = require("moon.tcpserver")
local socket = require("moon.socket")
local test_assert = require("test_assert")

--------------------------SERVER-------------------------
tcpserver.on("accept",function(sessionid, msg)
    --print("accept ", sessionid, msg:bytes())
end)

tcpserver.on("message",function(sessionid, msg)
    tcpserver.send_message(sessionid, msg)
end)

tcpserver.on("close",function(sessionid, msg)
    --print("close ", sessionid, msg:bytes())
end)

tcpserver.on("error",function(sessionid, msg)
    --print("error ", sessionid, msg:bytes())
end)

------------------------CLIENT----------------------------

local function send(session,data)
    if not session then
        return false
    end
    local len = #data
    return session:send(string.pack(">H",len)..data)
end

local function session_read( session )
    if not session then
        return false
    end
    local data,err = session:co_read(2)
    if not data then
        print(session.connid,"session read error",err)
        return false
    end

    local len = string.unpack(">H",data)

    data,err = session:co_read(len)
    if not data then
        print(session.connid,"session read error",err)
        return false
    end
    return data
end

local config

moon.init(function (cfg )
    config = cfg.network
    return true
end)

moon.start(function()
    local sock = socket.new()
    moon.async(function(  )
        for i=1,100 do
            local session,err = sock:co_connect(config.ip,config.port)
            if not session then
                print("connect failed", err)
                return
            end
            moon.async(function ()
                local send_data = tostring(session.connid)
                send(session, send_data)
                local rdata = session_read(session)
                test_assert.equal(rdata, send_data)
                if i == 100 then
                    test_assert.success()
                end
            end)
        end
    end)
end)
