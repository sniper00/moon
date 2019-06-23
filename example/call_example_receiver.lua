local moon   = require("moon")

local command = {}

command.ADD =  function(a,b)
    assert(false)
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

command.EXIT = function()
    moon.quit()
end

local function docmd(sender,sessionid, CMD,...)
    local f = command[CMD]
    if f then
        moon.response('lua',sender,sessionid,f(...))
    else
        error(string.format("Unknown command %s", tostring(CMD)))
    end
end

moon.dispatch('lua',function(msg,p)
    local sender = msg:sender()
    local sessionid = msg:sessionid()
    docmd(sender,sessionid, p.unpack(msg))
end)

