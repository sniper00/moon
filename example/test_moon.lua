local moon = require("moon")
local log = require("log")

local test_case =
{
    {
        name = "test_api",
        file = "test_api.lua"
    }
    ,
    {
        name = "test_network",
        file = "test_network.lua",
        network = {
            type = "listen",
            ip = "127.0.0.1",
            port =  "30001"
        }
    },
    {
        name = "test_send_sender",
        file = "test_send_sender.lua"
    }
    ,
    {
        name = "test_call_sender",
        file = "test_call_sender.lua"
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

command.SUCCESS = function(name)
    print("SUCCESS",name)
    next_case()
end

command.FAILED = function(name, dsp)
    log.error("FAILED %s %s", tostring(name), tostring(dsp))
    next_case()
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