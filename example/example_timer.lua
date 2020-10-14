local moon = require("moon")

local count = 10
moon.repeated(1000,count,function()
    print("repeate 10 times timer tick",count)
    count = count - 1
    if 0 == count then
        moon.exit(-1)
    end
end)

local count2 = 1
moon.repeated(1000,-1,function(timerid)
    print("infinite timer tick",count2)
    count2 = count2 + 1
    if count2 >5 then
        moon.remove_timer(timerid)
        print("remove infinite timer ")
    end
end)

moon.async(function()
    print("coroutine timer start")
    moon.sleep(1000)
    print("coroutine timer tick 1 seconds")
    moon.sleep(1000)
    print("coroutine timer tick 1 seconds")
    moon.sleep(1000)
    print("coroutine timer tick 1 seconds")
    moon.sleep(1000)
    print("coroutine timer tick 1 seconds")
    moon.sleep(1000)
    print("coroutine timer tick 1 seconds")
    print("coroutine timer end")
end)





