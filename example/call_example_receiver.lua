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

local function docmd(sender,responseid, CMD,...)
    local f = command[CMD]
    if f then
        moon.response('lua',sender,responseid,f(...))
    else
        error(string.format("Unknown command %s", tostring(CMD)))
    end
end

moon.dispatch('lua',function(msg,p)
    local sender = msg:sender()
    local responseid = msg:responseid()
    docmd(sender,responseid, p.unpack(msg:bytes()))
end)

