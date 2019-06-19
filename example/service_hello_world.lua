local moon = require("moon")

-- 演示服务的几个重要回掉的使用

moon.init(function(conf)
    print("service init", conf)

    -- 在此处编写初始化服务逻辑
    -- 如 注册消息回掉
    -- moon.dispatch("lua",function(msg,p)
    --      print(msg:sender())
    --      print(msg:receiver())
    --      print(msg:sessionid())
    --      print(msg:bytes())
    --      print(msg:header())
    -- end)

    return true --表示初始化成功
end)

moon.start(function()
    --此处可以访问其他服务
    local serviceid = moon.queryservice("gate service's name")
    print(serviceid)
    serviceid = moon.queryservice("db service's name")
    print(serviceid)
    serviceid = moon.queryservice("login service's name")
    print(serviceid)
    --[[ example
            moon.send("lua",serviceid,nil,"hello world")

            moon.async(function()
                moon.call("lua",serviceid,"hello world")
            end)
    ]]
end)

moon.exit(function()
    print("recive exit signal")
    --此处可以处理一些退出前的逻辑,可以有异步操作
    moon.async(function()
        -- moon.co_call("lua",db_service_id_1,"saveall")
        moon.co_wait(1000)
        -- moon.co_call("lua",db_service_id_2,"saveall")
        moon.co_wait(1000)
        -- moon.co_call("lua",db_service_id_3,"saveall")
        moon.co_wait(1000)
        moon.quit()
    end)
end)

moon.destroy(function()
    --服务真正退出,不能有异步操作
    print("service destroy")
end)
