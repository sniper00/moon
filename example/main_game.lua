---__init__---  初始化进程配置标识
if _G["__init__"] then
    local arg = ... ---这里可以获取命令行参数
    return {
        thread = 8, ---启动8条线程
        enable_stdout = true,
        logfile = string.format("log/game-%s.log", os.date("%Y-%m-%d-%H-%M-%S")),
        loglevel = "DEBUG", ---默认日志等级
        path = table.concat({
            "./?.lua",
            "./?/init.lua",
            "../lualib/?.lua",   -- moon lualib 搜索路径
            "../service/?.lua",  -- moon 自带的服务搜索路径，需要用到redisd服务
            -- Append your lua module search path
        }, ";")
    }
end

local moon = require("moon")

local socket = require "moon.socket"

--初始化服务配置
local db_conf= {host = "127.0.0.1", port = 6379, timeout = 1000}

local gate_host = "0.0.0.0"
local gate_port = 8889
local client_timeout = 300

local services = {
    {
        unique = true,
        name = "db",
        file = "../service/redisd.lua",
        threadid = 1, ---独占线程
        poolsize = 5, ---连接池
        opts = db_conf
    },
    {
        unique = true,
        name = "center",
        file = "game/service_center.lua",
        threadid = 2,
    },
}

moon.async(function ()
    for _, one in ipairs(services) do
        local id = moon.new_service( one)
        if 0 == id then
            moon.exit(-1) ---如果唯一服务创建失败，立刻退出进程
            return
        end
    end

    local listenfd = socket.listen(gate_host, gate_port, moon.PTYPE_SOCKET_TCP)
    if 0 == listenfd then
        moon.exit(-1) ---监听端口失败，立刻退出进程
        return
    end

    print("server start", gate_host, gate_port)

    while true do
        local id = moon.new_service( {
            name = "user",
            file = "game/service_user.lua"
        })

        local fd, err = socket.accept(listenfd, id)
        if not fd then
            print("accept",err)
            moon.kill(id)
        else
            moon.send("lua", id,"start", fd, client_timeout)
        end
    end

end)

moon.shutdown(function ()
    moon.async(function ()
        assert(moon.call("lua", moon.queryservice("center"), "shutdown"))
        moon.send("lua", moon.queryservice("db"), "save_then_quit")

        ---wait all service quit
        while true do
            local size = moon.server_stats("service.count")
            if size == 1 then
                break
            end
            moon.sleep(200)
            print("bootstrap wait all service quit, now count:", size)
        end

        moon.quit()
    end)
end)

