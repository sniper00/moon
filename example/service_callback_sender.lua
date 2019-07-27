local moon = require("moon")

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
    moon.async(function()
        local receiver =  moon.co_new_service("lua", {
            name = "service_callback_receiver",
            file = "service_callback_receiver.lua"
        })

        print(moon.name(), "send to", receiver, "command", "PING")
        moon.send('lua', receiver,"PING","Hello")
    end)
end)



