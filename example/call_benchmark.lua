local moon = require("moon")

local arg = ...

local nsender = 2000
local onecount = 1000

if arg and arg.runner then
    if arg.type == "receiver" then
        moon.dispatch("lua", function(msg, unpack)
            local sender, session, sz, len = moon.decode(msg, "SEC")
            moon.response("lua", sender, session, unpack(sz, len))
        end)
        return
    else
        moon.dispatch("lua", function(msg, unpack)
            moon.async(function()
                for i=1,onecount do
                    local ok,err= moon.co_call("lua", arg.target, "hello")
                    assert(ok, err)
                end
                moon.send("lua", arg.main, moon.clock())
            end)
        end)
    end
else
    local counter = 0
    local stime = 0
    local endtime = 0
    moon.dispatch("lua", function(msg, unpack)
        local etime = unpack(moon.decode(msg, "C"))
        if etime > endtime then
            endtime = etime
        end
        counter = counter + 1
        if counter >= nsender then
            print( nsender ,"call" ,nsender, "per", onecount, "total",  nsender*onecount, "cost", endtime - stime)
        end
    end)

    moon.async(function()
        local sender_addrs = {}
        for i=1,nsender do
            local receiver = moon.new_service("lua", {
                name="test",
                file = "call_benchmark.lua",
                runner = true,
                type = "receiver"
            })

            local addr = moon.new_service("lua", {
                name="test",
                file = "call_benchmark.lua",
                runner = true,
                type = "sender",
                target = receiver,
                main = moon.addr()
            })
            sender_addrs[#sender_addrs+1] = addr
        end

        moon.warn("call start")
        stime = moon.clock()
        for k,v in ipairs(sender_addrs) do
            moon.send("lua", v, "start")
        end
    end)
end