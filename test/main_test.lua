local moon = require("moon")


local test_case =
{
    {
        name = "core",
        file = "core.lua"
    }
    ,
    {
        name = "network",
        file = "network.lua",
        type = "listen",
        ip = "127.0.0.1",
        port =  "30001"
    },
    {
        name = "send",
        file = "send.lua"
    }
    ,
    {
        name = "call",
        file = "call.lua"
    }
    -- ,
    -- {
    --     name = "redis",
    --     file = "redis.lua"
    -- }
    ,
    {
        name = "large_package",
        file = "large_package.lua",
        ip = "127.0.0.1",
        port =  "30002",
        frame_flag= "rw"
    }
    ,
    {
        name = "http",
        file = "http.lua"
    }
    ,
    {
        name = "zset",
        file = "test_zset.lua"
    }
    ,
    {
        name = "json",
        file = "json.lua"
    }
}

local next_case = function ()
    local cfg = table.remove(test_case,1)
    if cfg then
        moon.async(function ()
            moon.new_service("lua",cfg)
        end)
    else
        moon.exit(0)
    end
end


local command = {}

command.SUCCESS = function(name,sid)
    print("SUCCESS",name)
    moon.async(function ()
        moon.kill(sid)
        next_case()
    end)
end

command.FAILED = function(name, sid, dsp)
    moon.error(string.format("FAILED %s %s", tostring(name), tostring(dsp)))
    moon.async(function ()
        moon.kill(sid)
        next_case()
    end)
end

moon.dispatch('lua',function(sender, session, cmd, ...)
    local f = command[cmd]
    if f then
        f(...)
    else
        error(string.format("Unknown command %s", tostring(cmd)))
    end
end)

next_case()
