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

    if count == 100000 then
        print("per", (t-bt))
        count = 0
        bt = 0
    end
end

moon.repeated(1000,-1,function(arg1, arg2, arg3)
    print(tcount)
end)

local function docmd(sender,sessionid, CMD,...)
    local f = command[CMD]
    if f then
        if CMD ~= 'ADD' then
            local args = {...}
            moon.async(function ()
                --moon.sleep(20000)
                moon.response('lua',sender,sessionid,f(table.unpack(args)))
            end)
        end
    else
        error(string.format("Unknown command %s", tostring(CMD)))
    end
end

moon.dispatch('lua',function(msg,p)
    local sender = msg:sender()
    local sessionid = msg:sessionid()
    docmd(sender,sessionid, p.unpack(msg))
end)

