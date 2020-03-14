local moon = require("moon")
local test_assert = require("test_assert")

local conf = ...

if conf.receiver then
    local command = {}

    command.HELLO = function(sender, ...)
        moon.send('lua', sender,'WORLD', ...)
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

else
    local send_content = "123456789"

    local command = {}

    command.WORLD = function(s)
        test_assert.equal(s, send_content)
        test_assert.success()
    end

    local function docmd(header, ...)
        local f = command[header]
        if f then
            f(...)
        else
            error(string.format("Unknown command %s", tostring(header)))
        end
    end

    moon.dispatch(
        "lua",
        function(msg, p)
            local header = msg:header()
            docmd(header, p.unpack(msg))
        end
    )

    local receiver

    moon.start(
        function()
            moon.async(function ()
                receiver = moon.co_new_service(
                    "lua",
                    {
                        name = "test_send",
                        file = "start_by_config/test_send.lua",
                        receiver = true
                    }
                )
                moon.send("lua", receiver, "HELLO", "123456789")
            end)
        end
    )

    moon.destroy(function()
        moon.remove_service(receiver)
    end)
end


