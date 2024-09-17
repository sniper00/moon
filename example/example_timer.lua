local moon = require("moon")

local timerid = moon.timeout(1000, function()
    error("must not print")
end)
moon.remove_timer(timerid)

moon.async(function()
    print("coroutine timer start")
    moon.sleep(1000, "tag1")
    for i=1,100000000 do
        -- body
    end
    print("coroutine timer tick 1 seconds")
    moon.sleep(1000, "tag2")
    print("coroutine timer tick 1 seconds")
    moon.sleep(1000)
    print("coroutine timer tick 1 seconds")
    moon.sleep(1000)
    print("coroutine timer tick 1 seconds")
    moon.sleep(1000)
    print("coroutine timer tick 1 seconds")
    print("coroutine timer end")
    moon.quit()
end)

local has_timeout = false
local co = moon.async(function()
    print("wakeup", moon.sleep(10000))
    has_timeout = true
end)

moon.async(function ()
    print("normal", moon.sleep(1000))

    if not has_timeout then
        moon.wakeup(co)
    end
end)
