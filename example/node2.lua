---__init__---
if _G["__init__"] then
    local arg = ...
    return {
        thread = 2,
        enable_stdout = true,
        logfile = string.format("log/node-%s-%s.log", arg[1], os.date("%Y-%m-%d-%H-%M-%S")),
        loglevel = "DEBUG",
        path = table.concat({ -- Define lua module search dir, all services use same lua search path
            "./?.lua",
            "./?/init.lua",
            "../lualib/?.lua",
            "../service/?.lua",
            -- Append your lua module search path
        }, ";")

        --path = "./?.lua;./?/init.lua;../lualib/?.lua;../service/?.lua；"
    }
end

local moon = require("moon")

local socket = require "moon.socket"
local json = require("json")
local httpc = require("moon.http.client")

local arg = moon.args()

local NODE_ETC_HOST = "127.0.0.1:9090"

local function run_recv_message()
    --- recv message from other node
    local command = {}

    command.ADD =  function(a,b)
        return a+b
    end

    command.SUB = function(a,b)
        return a-b
    end

    command.MUL = function(a,b)
        return a*b
    end

    command.ACCUM = function(...)
        local numbers = {...}
        local total = 0
        for _,v in pairs(numbers) do
            total = total + v
        end
        return total
    end

    local count = 0
    local tcount = 0
    local bt = 0
    command.COUNTER = function(t)
        -- print(...)
        count = count + 1
        if bt == 0 then
            bt = t
        end
        tcount = tcount + 1

        if count == 10000 then
            print("10000 cost", (t-bt), "ms")
            count = 0
            bt = 0
        end
    end

    moon.dispatch('lua',function(sender, session, CMD, ...)
        local f = command[CMD]
        if f then
            if CMD ~= 'ADD' then
                --moon.sleep(20000)
                moon.response('lua',sender, session,f(...))
            end
        else
            error(string.format("Unknown command %s", tostring(CMD)))
        end
    end)
end

local function init(node_conf)

    local services = {
        {
            unique = true,
            name = "cluster",
            file = "../service/cluster.lua",
            etc_host = NODE_ETC_HOST,
            etc_path = "/cluster?node=%s",
            threadid = 2,
        }
    }

    local server_ok = false
    local addrs = {}

    moon.async(function()
        for _, conf in ipairs(services) do
            local addr = moon.new_service(conf)
            ---如果关键服务创建失败，立刻退出进程
            if 0 == addr then
                moon.exit(-1)
                return
            end
            table.insert(addrs, addr)
        end
        server_ok = true

        -- cluster服务开启监听端口
        print(moon.call("lua", moon.queryservice("cluster"), "Listen"))

        run_recv_message()
    end)

    ---注册进程退出信号处理
    moon.shutdown(function()
        print("receive shutdown")
        moon.async(function()
            if server_ok then
                moon.kill(moon.queryservice("cluster"))
            else
                moon.exit(-1)
            end
            moon.quit()
        end)
    end)
end

moon.async(function()
    local response = httpc.get(NODE_ETC_HOST, {
        path = "/conf.node?node=" .. tostring(arg[1])
    })

    if response.status_code ~= 200 then
        moon.error(response.status_code, response.content)
        moon.exit(-1)
        return
    end

    local node_conf = json.decode(response.content)

    -- 设置当前节点环境变量，cluster服务需要用到
    moon.env("NODE", arg[1])

    init(node_conf)
end)