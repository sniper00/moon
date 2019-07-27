local moon = require("moon")

moon.start(function()

    local count = 10
    moon.repeated(1000,count,function()
        print("repeate 10 times timer tick",count)
        count = count - 1
        if 0 == count then
            moon.abort()
        end
    end)

    moon.async(function()
        print("coroutine timer start")
        moon.co_wait(1000)
        print("coroutine timer tick 1 seconds")
        moon.co_wait(1000)
        print("coroutine timer tick 1 seconds")
        moon.co_wait(1000)
        print("coroutine timer tick 1 seconds")
        moon.co_wait(1000)
        print("coroutine timer tick 1 seconds")
        moon.co_wait(1000)
        print("coroutine timer tick 1 seconds")
        print("coroutine timer end")
    end)
end)



