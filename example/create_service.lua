local moon = require("moon")

moon.start(function()

    -- 动态创建服务, 配置同时可以用来传递一些信息
    moon.new_service("lua", {
        name = "create_service2",
        file = "create_service2.lua",
        message = "Hello create_service"
    })

    moon.async(function()
        -- 动态创建服务，协程方式等待获得服务ID，方便用来通信
        local serviceid =  moon.co_new_service("lua", {
            name = "create_service2",
            file = "create_service2.lua",
            message = "Hello create_service coroutine"
        })

        --moon.send("lua", serviceid, "ADD", 1, 2)

        print("co_new_service service:", serviceid)
    end)

    print("enter 'CTRL-C' stop server.")
end)


