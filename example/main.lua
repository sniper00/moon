local json = require("json")

local get_env = _G.get_env
local set_env = _G.set_env
local new_service = _G.new_service

-- define lua module search dir
local path = "../lualib/?.lua;../service/?.lua;start_by_config/?.lua;"

-- define lua c module search dir
local cpath = "../clib/?.dll;../clib/?.so;"

set_env("PATH", path)
set_env("CPATH", cpath)

package.path = path .. package.path
package.cpath = cpath .. package.cpath

local params = json.decode(get_env("PARAMS"))

local sid = math.tointeger(get_env("SID"))

local services

local switch = {}

switch[1] = function ()
    services = {
        {
            unique = true,
            name = "network",
            file = "start_by_config/network.lua",
            host = params.host,
            port = params.port
        },
        {
            unique = true,
            name = "network_websocket",
            file = "start_by_config/network_websocket.lua",
            host = params.host,
            port = params.wsport
        }
    }
end

switch[2] = function ()
    services = {
        {
            unique = true,
            name = "test",
            file = "start_by_config/test_moon.lua",
            threadid = 1
        }
    }
end

switch[3] = function ()
    services = {
        {
            unique = true,
            name = "send_benchmark_sender",
            file = "start_by_config/send_benchmark_sender.lua",
        },
        {
            unique = true,
            name = "send_benchmark_receiver1",
            file = "start_by_config/send_benchmark_receiver.lua",
        },    {
            unique = true,
            name = "send_benchmark_receiver2",
            file = "start_by_config/send_benchmark_receiver.lua",
        }
        ,
        {
            unique = true,
            name = "send_benchmark_receiver3",
            file = "start_by_config/send_benchmark_receiver.lua",
        }
        ,
        {
            unique = true,
            name = "send_benchmark_receiver4",
            file = "start_by_config/send_benchmark_receiver.lua",
        }
    }
end

switch[4] = function ()
    services = {
        {
            unique = true,
            name= "master",
            file = "start_by_config/network_text_benchmark.lua",
            host = "127.0.0.1",
            port = 42345,
            master = true,
            count = 4
        }
    }
end

switch[5] = function ()
    services = {
        {
            unique = true,
            name= "server",
            file = "start_by_config/network_benchmark.lua",
            host = "127.0.0.1",
            port = 42346,
            master = true,
            count = 4
        },
        {
            unique = true,
            name= "client",
            file = "start_by_config/network_benchmark_client.lua",
            host = "127.0.0.1",
            port = 42346,
            client_num = 1000,
            count = 100
        }
    }
end

switch[6] = function ()
    services = {
        {
            unique = true,
            name = "mysql",
            file = "start_by_config/service_mysql.lua",
            host = "127.0.0.1",
            port = 3306,
            database = "mysql",
            user="root",
            password="4321",
            timeout= 1000,
            max_packet_size=102400,
            connection_num=4
        },
        {
            unique = true,
            name = "call_mysql_service",
            file = "start_by_config/call_mysql_service.lua"
        }
    }
end

switch[7] = function ()
    services = {
        {
            unique = true,
            name = "redis",
            file = "start_by_config/service_redis.lua",
            host = "127.0.0.1",
            port = 3306,
            timeout = 1000,
            connection_num = 4
        },
        {
            unique = true,
            name = "call_redis_service",
            file = "start_by_config/call_redis_service.lua"
        }
    }
end

switch[8] = function ()
    services = {
        {
            unique = true,
            name = "sharetable",
            file = "../service/sharetable.lua"
        },
        {
            unique = true,
            name = "sharetable_example",
            file = "start_by_config/sharetable_example.lua"
        }
    }
end

switch[9] = function ()
    services = {
        {
            unique = true,
            name = "cluster",
            file = "../service/cluster.lua",
            host = params.cluster_host,
            port = params.cluster_port
        },
        {
            unique = true,
            name = "cluster_receiver",
            file = "start_by_config/cluster_receiver.lua"
        }
    }
end

switch[10] = function ()
    services = {
        {
            unique = true,
            name = "cluster",
            file = "../service/cluster.lua",
            host = params.cluster_host,
            port = params.cluster_port
        },
        {
            unique = true,
            name = "cluster_sender",
            file = "start_by_config/cluster_sender.lua"
        }
    }
end

local fn = switch[sid]
if not fn then
    return 0
end

fn()

for _, conf in ipairs(services) do
    local service_type = conf.service_type or "lua"
    local unique = conf.unique or false
    local threadid = conf.threadid or 0

    new_service(service_type, json.encode(conf), unique, threadid, 0 ,0 )
end

return #services