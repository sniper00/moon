local moon = require("moon")
local test_assert = require("test_assert")

local conf = ...

if conf and conf.receiver then
    local command = {}

    command.HELLO = function(sender, ...)
        moon.send('lua', sender,'WORLD', ...)
    end

    moon.dispatch('lua',function(sender,session, cmd, ...)
        -- body
        local f = command[cmd]
        if f then
            f(sender,...)
        else
            error(string.format("Unknown command %s", tostring(cmd)))
        end
    end)

else
    local send_content = "123456789"

    local command = {}

    command.WORLD = function(s)
        test_assert.equal(s, send_content)
        test_assert.success()
    end

    moon.dispatch("lua",function(sender, session, cmd, ...)
        local f = command[cmd]
        if f then
            f(...)
        else
            error(string.format("Unknown command %s", tostring(cmd)))
        end
    end)

    local receiver

    moon.async(function ()
        receiver = moon.new_service(
            {
                name = "test_send",
                file = "send.lua",
                receiver = true
            }
        )
        moon.send("lua", receiver, "HELLO", "123456789")
    end)

    moon.shutdown(function()
        moon.kill(receiver)
    end)
end


