local moon = require("moon")

local conf = ...

if conf and conf.receiver then
    -----------------------------THIS IS RECEIVER SERVICE-------------------
    local command = {}

    command.PING = function(sender, ...)
        print(moon.name, "recv ", sender, "command", "PING")
        print(moon.name, "send to", sender, "command", "PONG")
        moon.send('lua', sender,'PONG', ...)
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

    moon.dispatch('lua',function(sender, session, cmd, ...)
        local f = command[cmd]
        if f then
            f(sender,...)
        else
            error(string.format("Unknown command %s", tostring(cmd)))
        end
    end)

    print("callback example: service receiver start")
else
    -----------------------------THIS IS SENDER SERVICE-------------------
    local command = {}

    command.PONG = function(...)
        print(...)
        print(moon.name, "recv ", "command", "PING")
        moon.exit(-1)
    end

    moon.dispatch('lua',function(sender, session, cmd, ...)
        local f = command[cmd]
        if f then
            f(...)
        else
            error(string.format("Unknown command %s", tostring(cmd)))
        end
    end)

    print("callback example: service sender start")

    moon.async(function()
        local receiver =  moon.new_service("lua", {
            name = "callback_receiver",
            file = "example_callback.lua",
            receiver = true
        })

        print(moon.name, "send to", receiver, "command", "PING")
        moon.send('lua', receiver,"PING","Hello")
    end)
end




