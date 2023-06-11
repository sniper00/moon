---__init__
if _G["__init__"] then
    local arg = ...
    return {
        thread = 2,
        enable_stdout = true,
        logfile = string.format("log/node-%s-%s.log", arg[1], os.date("%Y-%m-%d-%H-%M-%S")),
        loglevel = "DEBUG",
    }
end

-- Define lua module search dir, all services use same lua search path
local path = table.concat({
    "./?.lua",
    "./?/init.lua",
    "../lualib/?.lua",
    "../service/?.lua",
    -- Append your lua module search path
}, ";")

package.path = path .. ";"

local moon = require("moon")
moon.env("PATH", string.format("package.path='%s'", package.path))

local socket = require "moon.socket"
local json = require("json")
local httpc = require("moon.http.client")

local arg = moon.args()

local NODE_ETC_HOST = "127.0.0.1:9090"

local function run_send_message()
    --- send message to target node
    local cluster = require("cluster")

    local target_node_id = 2

    local counter = 0

    moon.async(function()
        while true do
            moon.sleep(1000)
            print("per sec ",counter)
            counter=0
        end
    end)

    local args = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}
    moon.async(function()

        print(cluster.call(target_node_id, 'bootstrap', "ACCUM", table.unpack(args)))
        for i=1,100000 do
            cluster.send(target_node_id, 'bootstrap',"COUNTER", moon.now())
        end

        while true do
            local ret ,err = cluster.call(target_node_id, 'bootstrap', "ACCUM", table.unpack(args))
            if not ret then
                print(err)
                return
            end
            counter=counter+1
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

    moon.async(function ()
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

        --print(moon.call("lua", moon.queryservice("cluster"), "Listen"))

        run_send_message()
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