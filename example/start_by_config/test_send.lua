local moon = require("moon")
local test_assert = require("test_assert")

local conf = ...

if conf.receiver then
    local command = {}

    command.HELLO = function(sender, ...)
        moon.send('lua', sender,'WORLD', ...)
    end

    local function docmd(sender,cmd,...)
        -- body
        local f = command[cmd]
        if f then
            f(sender,...)
        else
            error(string.format("Unknown command %s", tostring(cmd)))
        end
    end

    moon.dispatch('lua',function(msg,unpack)
        local sender, sz, len = moon.decode(msg, "SC")
        docmd(sender, unpack(sz, len))
    end)

else
    local send_content = "123456789"

    local command = {}

    command.WORLD = function(s)
        test_assert.equal(s, send_content)
        test_assert.success()
    end

    local function docmd(cmd, ...)
        local f = command[cmd]
        if f then
            f(...)
        else
            error(string.format("Unknown command %s", tostring(cmd)))
        end
    end

    moon.dispatch(
        "lua",
        function(msg, unpack)
            local sz, len = moon.decode(msg, "C")
            docmd(unpack(sz, len))
        end
    )

    local receiver

    moon.async(function ()
        receiver = moon.new_service(
            "lua",
            {
                name = "test_send",
                file = "start_by_config/test_send.lua",
                receiver = true
            }
        )
        moon.send("lua", receiver, "HELLO", "123456789")
    end)

    moon.shutdown(function()
        moon.remove_service(receiver)
    end)
end


