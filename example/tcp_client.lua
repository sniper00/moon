------------------------TCP CLIENT----------------------------
local moon = require("moon")
local socket = require("moon.net.socket")

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
        local session,err = sock:co_connect(config.ip,config.port)
        if not session then
            print("connect failed", err)
            return
        end
        moon.async(function ()
            print("Please input:")
            repeat
                local input = io.read()
                if input then
                    send(session, input)
                    local rdata = session_read(session)
                    print("recv", rdata)
            end
            until(not input)
        end)
    end)
end)