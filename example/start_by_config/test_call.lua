local moon = require("moon")
local test_assert = require("test_assert")
local conf = ...

if conf.receiver then
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
    end

    local function docmd(sender,sessionid, CMD,...)
        local f = command[CMD]
        if f then
            moon.response('lua',sender,sessionid,f(...))
        else
            error(string.format("Unknown command %s", tostring(CMD)))
        end
    end

    moon.dispatch('lua',function(msg,p)
        local sender = msg:sender()
        local sessionid = msg:sessionid()
        docmd(sender,sessionid, p.unpack(msg))
    end)

else
    moon.start(
        function()
            moon.async(
                function()
                    local receiverid =
                    moon.co_new_service(
                        "lua",
                        {
                            name = "test_call_receiver",
                            file = "start_by_config/test_call.lua",
                            receiver = true
                        }
                    )

                    print(moon.co_call("lua", receiverid, "SUB", 1000, 2000))

                    local res = moon.co_call("lua", receiverid, "SUB", 1000, 2000)
                    test_assert.equal(res, -1000)

                    res = moon.co_call("lua", receiverid, "ACCUM", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)
                    test_assert.equal(res, 136)

                    --This call will got error message:
                    res = moon.co_call("lua", receiverid, "ADD", 100, 99)
                    test_assert.equal(res, false)

                    res = moon.co_call("lua", receiverid, "SUB", 100, 99)
                    test_assert.equal(res, 1)

                    --Let receiver exit:
                    moon.co_call("lua", receiverid, "EXIT")
                    res = moon.co_call("lua", receiverid, "SUB", 100, 99)
                    test_assert.equal(res, false)
                    test_assert.success()
                end
            )
        end
    )

end

