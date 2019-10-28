local moon = require("moon")
local conf = ...

local command = {}

command.QUIT = function ()
    print("recv quit cmd, bye bye")
    moon.quit()
end

local function docmd(sender,header,...)
-- body
    local f = command[header]
    if f then
        f(sender,...)
    else
        error(string.format("Unknown command %s", tostring(header)))
    end
end

moon.start(function()
    print("conf:", conf.message)

    moon.dispatch('lua',function(msg,p)
        local sender = msg:sender()
        local header = msg:header()
        docmd(sender,header, p.unpack(msg))
    end)

    if conf.auto_quit then
        print("auto quit, bye bye")
        -- 使服务退出
        moon.quit()
    end
end)