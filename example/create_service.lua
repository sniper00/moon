local moon = require("moon")

moon.start(function()

    -- 动态创建服务, 配置同时可以用来传递一些信息
    moon.new_service("lua", {
        name = "create_service2",
        file = "create_service2.lua",
        message = "Hello create_service",
        auto_quit = true
    })

    moon.async(function()
        -- 动态创建服务，协程方式等待获得服务ID，方便用来通信
        local serviceid =  moon.co_new_service("lua", {
            name = "create_service2",
            file = "create_service2.lua",
            message = "Hello create_service_coroutine"
        })

        print("new service",string.format("%X",serviceid))

        moon.send("lua", serviceid, "QUIT")

        moon.abort()
    end)
end)


