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
    local world_count = 0

    local command = {}

    command.WORLD = function(s)
        test_assert.equal(s, send_content)
        world_count = world_count + 1
        if world_count == 2 then
            test_assert.success()
        end
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
        local ok = pcall(moon.send, "unknown", 1, "HELLO")
        test_assert.assert(not ok, "moon.send should reject unknown PTYPE")

        ok = pcall(moon.send, "lua", 0, "HELLO")
        test_assert.assert(not ok, "moon.send should reject receiver 0")

        ok = pcall(moon.raw_send, "unknown", 1, "HELLO")
        test_assert.assert(not ok, "moon.raw_send should reject unknown PTYPE")

        ok = pcall(moon.raw_send, "lua", 0, "HELLO")
        test_assert.assert(not ok, "moon.raw_send should reject receiver 0")

        receiver = moon.new_service(
            {
                name = "test_send",
                file = "send.lua",
                receiver = true
            }
        )

        moon.send("lua", receiver, "HELLO", "123456789")

        local session, receiverid = moon.raw_send("lua", receiver, moon.pack("HELLO", send_content), 12345)
        test_assert.equal(session, 12345)
        test_assert.equal(receiverid, receiver)
    end)

    moon.shutdown(function()
        moon.kill(receiver)
    end)
end


