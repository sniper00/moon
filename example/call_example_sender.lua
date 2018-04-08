local moon = require("moon")

local receiverid

moon.start(function()

    receiverid= moon.new_service("lua", {
        name = "call_example_receiver",
        file = "call_example_receiver.lua"
    })

    moon.start_coroutine(function()
        print(moon.co_call('lua',receiverid,"SUB",1000,2000))

        moon.co_wait(1000)

        print(moon.co_call('lua',receiverid,"ACCUM",1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16))

        -- will return ,service exit/crash
        print(moon.co_call('lua',receiverid,"ADD",100,99))
        -- call exited service
        print(moon.co_call('lua',receiverid,"ADD",100,99))
    end)

end)




