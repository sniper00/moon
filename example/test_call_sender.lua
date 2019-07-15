local moon = require("moon")
local test_assert = require("test_assert")


moon.start(
    function()
        moon.async(
            function()
                local receiverid =
                moon.co_new_service(
                    "lua",
                    {
                        name = "test_call_receiver",
                        file = "test_call_receiver.lua"
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
                test_assert.success()
            end
        )
    end
)
