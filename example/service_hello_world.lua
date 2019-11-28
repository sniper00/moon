local moon = require("moon")

-- 获取服务的配置信息，table
local conf = ...
print_r(conf)

-- 在此处编写初始化服务逻辑,必要时添加assert断言,初始化失败的服务会立刻销毁
-- 如 注册消息回掉
-- moon.dispatch("lua",function(msg,p)
--      print(msg:sender())
--      print(msg:receiver())
--      print(msg:sessionid())
--      print(msg:bytes())
--      print(msg:header())
-- end)

-- 服务的几个重要回掉的示例

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
                moon.co_call("lua",serviceid,"hello world")
            end)
    ]]
end)

moon.exit(function()
    print("recive exit signal, 3 seconds later service quit.")
    --此处可以处理一些进程退出前的逻辑,可以有异步操作
    moon.async(function()
        -- moon.co_call("lua",db_service_id_1,"saveall")
        moon.sleep(1000)
        -- moon.co_call("lua",db_service_id_2,"saveall")
        moon.sleep(1000)
        -- moon.co_call("lua",db_service_id_3,"saveall")
        moon.sleep(1000)
        moon.quit()
    end)
end)

moon.destroy(function()
    --服务真正退出,不能有异步操作
    print("service destroy")
end)
