local moon = require("moon")
local test_assert = require("test_assert")
local conf = ...

if conf and conf.receiver then
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
        return true
    end

    moon.dispatch('lua',function(sender, session, cmd, ...)
        local f = command[cmd]
        if f then
            moon.response('lua',sender,session,f(...))
        else
            error(string.format("Unknown command %s", tostring(cmd)))
        end
    end)

else
    moon.async(
        function()
            local receiverid =
            moon.new_service(
                {
                    name = "test_call_receiver",
                    file = "call.lua",
                    receiver = true
                }
            )

            print(moon.call("lua", receiverid, "SUB", 1000, 2000))

            local res = moon.call("lua", receiverid, "SUB", 1000, 2000)
            test_assert.equal(res, -1000)

            res = moon.call("lua", receiverid, "ACCUM", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)
            test_assert.equal(res, 136)

            --This call will got error message:
            res = moon.call("lua", receiverid, "ADD", 100, 99)
            test_assert.equal(res, false)

            res = moon.call("lua", receiverid, "SUB", 100, 99)
            test_assert.equal(res, 1)

            --Let receiver exit:
            assert(moon.call("lua", receiverid, "EXIT"))
            res = moon.call("lua", receiverid, "SUB", 100, 99)
            test_assert.equal(res, false)
            test_assert.success()
        end
    )

end

