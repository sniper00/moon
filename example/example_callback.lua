local moon = require("moon")

local conf = ...

if conf.receiver then
    -----------------------------THIS IS RECEIVER SERVICE-------------------
    local command = {}

    command.PING = function(sender, ...)
        print(moon.name(), "recv ", sender, "command", "PING")
        print(moon.name(), "send to", sender, "command", "PONG")
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

    moon.dispatch('lua',function(msg,p)
        local sender = msg:sender()
        local header = msg:header()
        docmd(sender,header, p.unpack(msg))
    end)

    print("callback example: service receiver start")
else
    -----------------------------THIS IS SENDER SERVICE-------------------
    local command = {}

    command.PONG = function(...)
        print(...)
        print(moon.name(), "recv ", "command", "PING")
        moon.abort()
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
        print("callback example: service sender start")

        moon.async(function()
            local receiver =  moon.co_new_service("lua", {
                name = "callback_receiver",
                file = "example_callback.lua",
                receiver = true
            })

            print(moon.name(), "send to", receiver, "command", "PING")
            moon.send('lua', receiver,"PING","Hello")
        end)
    end)
end




