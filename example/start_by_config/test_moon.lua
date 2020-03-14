local moon = require("moon")
local log = require("moon.log")

local test_case =
{
    {
        name = "test_api",
        file = "start_by_config/test_api.lua"
    }
    ,
    {
        name = "test_network",
        file = "start_by_config/test_network.lua",
        network = {
            type = "listen",
            ip = "127.0.0.1",
            port =  "30001"
        }
    },
    {
        name = "test_send_sender",
        file = "start_by_config/test_send.lua"
    }
    ,
    {
        name = "test_call_sender",
        file = "start_by_config/test_call.lua"
    }
    ,
    {
        name = "test_redis",
        file = "start_by_config/test_redis.lua"
    }
    ,
    {
        name = "test_large_package",
        file = "start_by_config/test_large_package.lua",
        network = {
            ip = "127.0.0.1",
            port =  "30002",
            frame_flag= "rw"
        }
    }
    ,
    {
        name = "test_http",
        file = "start_by_config/test_http.lua"
    }
}

local next_case = function ()
    local cfg = table.remove(test_case,1)
    if cfg then
        moon.new_service("lua",cfg)
    else
        --print("abort")
        moon.abort()
    end
end


local command = {}

command.SUCCESS = function(name,sid)
    print("SUCCESS",name)
    moon.async(function ()
        moon.co_remove_service(sid)
        next_case()
    end)
end

command.FAILED = function(name, sid, dsp)
    log.error("FAILED %s %s", tostring(name), tostring(dsp))
    moon.async(function ()
        moon.co_remove_service(sid)
        next_case()
    end)
end

local function docmd(header,...)
    local f = command[header]
    if f then
        f(...)
    else
        error(string.format("Unknown command %s", tostring(header)))
    end
end

moon.dispatch('lua',function(msg,p)
    local header = msg:header()
    docmd(header, p.unpack(msg))
end)

moon.start(function()
    next_case()
end)