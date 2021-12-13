local moon   = require("moon")

local command = {}

command.ADD =  function(a,b)
    return a+b
end

command.SUB = function(a,b)
    return a-b
end

command.MUL = function(a,b)
    return a*b
end

command.ACCUM = function(...)
    local numbers = {...}
    local total = 0
    for _,v in pairs(numbers) do
        total = total + v
    end
    return total
end

local count = 0
local tcount = 0
local bt = 0
command.COUNTER = function(t)
    -- print(...)
    count = count + 1
    if bt == 0 then
        bt = t
    end
    tcount = tcount + 1

    if count == 10000 then
        print("per", (t-bt))
        count = 0
        bt = 0
    end
end

moon.async(function()
    while true do
        moon.sleep(1000)
        print(tcount)
    end
end)


moon.dispatch('lua',function(sender, session, CMD, ...)
    local f = command[CMD]
    if f then
        if CMD ~= 'ADD' then
            --moon.sleep(20000)
            moon.response('lua',sender, session,f(...))
        end
    else
        error(string.format("Unknown command %s", tostring(CMD)))
    end
end)

moon.async(function()
    moon.sleep(10)
    print(moon.co_call("lua", moon.queryservice("cluster"), "Start"))
end)

